// Copy the normal provider's archive-name return during its existing caller.
// Never invoke a host lookup, load data, mount an archive or change gameplay.
#include "../StateBridge/WindowsTarget.h"
#include "../StateBridge/AuditQueue.h"
#include "Source.h"
#include "MinHook.h"
#include <intrin.h>
#include <cwchar>

namespace {
ktguidesource::SourceFn originalSource=nullptr;
ktguidesource::LookupFn originalLookup=nullptr;
std::uintptr_t gameBase=0;HMODULE ownModule=nullptr;void* targets[2]{};
HANDLE writerThread=nullptr,logFile=INVALID_HANDLE_VALUE;
std::atomic<bool> attempted{false},recording{false},stopping{false},stopBusy{false},writeFailed{false},truncated{false};
std::atomic<unsigned> inFlight{0};
std::atomic<unsigned long long> calls[2]{},transactions{0},ended{0},dropped{0},reserved{0},committed{0},unlinked{0},overflow{0};
constexpr unsigned Capacity=256,MaxRecords=1024;
struct Sample {
 std::uint64_t transaction=0;long long begin=0,end=0;DWORD thread=0;
 unsigned depth=0,pathKind=0,sourceNamespace=0,lookups=0,matches=0,archive=0;std::int32_t index=0;
 bool complete=false,exceptional=false,suppressed=false,expectedKnown=false;
 ktguidesource::Result result=ktguidesource::Result::NotObserved;
};
struct Frame {Sample sample;char expected[260]{};};
struct Stack {unsigned depth=0,suppressed=0;Frame frame[ktguidesource::MaxDepth];};
thread_local Stack stack;
using Queue=kt::AuditQueue<Sample,Capacity>;Queue queue;
#ifdef KT_GUIDE_SOURCE_FIXTURE
thread_local bool fixtureCallers=false;
#endif
long long ticks(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
bool caller(std::uintptr_t address) {
#ifdef KT_GUIDE_SOURCE_FIXTURE
 if(fixtureCallers)return true;
#endif
 return address==gameBase+ktguidesource::LookupReturn;
}
enum class PathRead {Ok,Unreadable,Truncated,Bad};
// Exact slash/case normalization only. Do not resolve aliases, dot segments,
// arbitrary VFS roots or relative paths. Reject unsafe/ambiguous input instead.
PathRead pathCopy(const char* source,char* out,unsigned capacity) {
 __try {
  if(!source)return PathRead::Unreadable;
  unsigned length=0;
  for(;length<capacity;++length){char c=source[length];
   if(!c){out[length]=0;break;}
   if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)return PathRead::Bad;
   out[length]=c=='\\'?'/':c>='A'&&c<='Z'?static_cast<char>(c+32):c;
  }
  if(length==capacity){out[0]=0;return PathRead::Truncated;}
  for(unsigned start=0;start<length;){unsigned end=start;while(end<length&&out[end]!='/')++end;
   if((end-start==1&&out[start]=='.')||(end-start==2&&out[start]=='.'&&out[start+1]=='.'))return PathRead::Bad;
   start=end+1;
  }
  return PathRead::Ok;
 }__except(EXCEPTION_EXECUTE_HANDLER){out[0]=0;return PathRead::Unreadable;}
}
bool suffix(const char* path,const char* expected) {
 const auto n=strlen(path),m=strlen(expected);return n>=m&&!memcmp(path+n-m,expected,m)&&(n==m||path[n-m-1]=='/');
}
unsigned archiveKind(const char* path) {
 if(!*path)return 0;
 const auto n=strlen(path);if(n<4||strcmp(path+n-4,".pak"))return 4;
 const char* own=strstr(path,"mods/kuttenberg_thief/");
 if(own&&(own==path||own[-1]=='/'))return 2;
 const char* mods=strstr(path,"mods/");if(mods&&(mods==path||mods[-1]=='/'))return 3;
 if(suffix(path,"data/levels/kutnohorsko/level.pak"))return 1;
 return 3;
}
struct FileIdentity {char logical[260]{};std::uintptr_t handle=0,provider=0,table=0,implementation=0;};
bool identity(void* file,FileIdentity& out) {
 if(pathCopy(static_cast<const char*>(file),out.logical,sizeof out.logical)!=PathRead::Ok)return false;
 __try {
  if(!file)return false;const auto f=static_cast<unsigned char*>(file);
  out.handle=*reinterpret_cast<std::uintptr_t*>(f+0x108);out.provider=*reinterpret_cast<std::uintptr_t*>(f+0x110);
  if(out.provider){out.table=*reinterpret_cast<std::uintptr_t*>(out.provider);if(out.table)out.implementation=*reinterpret_cast<std::uintptr_t*>(out.table+0x248);}
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void enqueue(const Sample& sample) {
 auto n=reserved.load();do{if(n>=MaxRecords){truncated=true;recording=false;return;}}while(!reserved.compare_exchange_weak(n,n+1));
 Queue::Ticket t;if(queue.reserve(t)!=Queue::Result::Reserved){++dropped;recording=false;return;}queue.commit(t,sample);++committed;
}
Frame* enter(const char* path,std::uintptr_t index) {
 if(!recording.load()||stack.suppressed){++stack.suppressed;return nullptr;}
 if(stack.depth==ktguidesource::MaxDepth){stack.frame[stack.depth-1].sample.suppressed=true;++overflow;recording=false;++stack.suppressed;return nullptr;}
 auto& f=stack.frame[stack.depth++];f=Frame{};f.sample.transaction=++transactions;f.sample.begin=ticks();
 f.sample.thread=GetCurrentThreadId();f.sample.depth=stack.depth;f.sample.index=static_cast<std::int32_t>(index);
 char folder[260]{};
 if(pathCopy(path,folder,sizeof folder)==PathRead::Ok&&*folder){
  f.sample.pathKind=suffix(folder,"kutnohorsko")?1:2;
  f.sample.sourceNamespace=!strcmp(folder,"levels/kutnohorsko")?1:!strcmp(folder,"data/levels/kutnohorsko")?2:3;
  char member[32];sprintf_s(member,"/whdata_%d",f.sample.index);
  if(strlen(folder)+strlen(member)<sizeof f.expected){strcpy_s(f.expected,folder);strcat_s(f.expected,member);f.sample.expectedKnown=true;}
 }
 return &f;
}
void leave(Frame* f,bool complete) {
 if(!f){if(stack.suppressed)--stack.suppressed;return;}
 f->sample.complete=complete;f->sample.exceptional=!complete;f->sample.end=ticks();
 enqueue(f->sample);memset(f->expected,0,sizeof f->expected);++ended;--stack.depth;
}
void __fastcall observedSource(void* a,const char* path,std::uintptr_t c,std::uintptr_t index) {
 ++inFlight;++calls[0];auto f=enter(path,index);bool complete=false;
 __try {originalSource(a,path,c,index);complete=true;}
 __finally {leave(f,complete);--inFlight;}
}
const char* __fastcall observedLookup(void* file) {
 ++inFlight;++calls[1];auto f=stack.depth&&!stack.suppressed?&stack.frame[stack.depth-1]:nullptr;
 FileIdentity before;bool linked=false;
 if(f){++f->sample.lookups;
  if(!caller(reinterpret_cast<std::uintptr_t>(_ReturnAddress())))f->sample.result=ktguidesource::Result::Caller;
  else if(!f->sample.expectedKnown)f->sample.result=ktguidesource::Result::PathUnknown;
  else if(!identity(file,before))f->sample.result=ktguidesource::Result::Unreadable;
  else if(strcmp(before.logical,f->expected))f->sample.result=ktguidesource::Result::MemberMismatch;
  else {++f->sample.matches;
   if(!before.handle||!before.provider||before.table!=gameBase+ktguidesource::ProviderVtable||before.implementation!=gameBase+ktguidesource::ProviderLookup)f->sample.result=ktguidesource::Result::ProviderUnknown;
   else linked=true;
  }
 }else ++unlinked;
 const char* result=nullptr;
 __try {
  result=originalLookup(file);
  if(linked){FileIdentity after;
   if(!identity(file,after))f->sample.result=ktguidesource::Result::Unreadable;
   else if(strcmp(after.logical,before.logical)||after.handle!=before.handle||after.provider!=before.provider||after.table!=before.table||after.implementation!=before.implementation)f->sample.result=ktguidesource::Result::Changed;
   else {char path[512]{};const auto read=pathCopy(result,path,sizeof path);
    if(read==PathRead::Unreadable)f->sample.result=ktguidesource::Result::Unreadable;
    else if(read==PathRead::Truncated)f->sample.result=ktguidesource::Result::Truncated;
    else if(read==PathRead::Bad)f->sample.result=ktguidesource::Result::BadPath;
    else if(!*path)f->sample.result=ktguidesource::Result::Empty;
    else {f->sample.archive=archiveKind(path);f->sample.result=ktguidesource::Result::Observed;}
   }
  }
 }__finally{--inFlight;}
 return result;
}
bool writeLine(const char* line){DWORD written=0;auto n=static_cast<DWORD>(strlen(line));if(!WriteFile(logFile,line,n,&written,nullptr)||written!=n){writeFailed=true;recording=false;return false;}return true;}
bool writeHeader(){return writeLine("{\"event\":\"started\",\"probe\":\"guide-source\",\"protocol\":1,\"motion_compiled\":false,\"mounts_archives\":false,\"registers_guides\":false,\"record_limit\":1024,\"logical_path_capacity\":260,\"archive_path_capacity\":512}\n");}
DWORD WINAPI writer(void*) {
 char line[2048];Sample s;
 for(;;){while(queue.pop(s)){
  const bool qualified=s.complete&&!s.exceptional&&!s.suppressed&&s.expectedKnown&&s.lookups==1&&s.matches==1&&s.result==ktguidesource::Result::Observed&&s.archive>=1&&s.archive<=3;
  sprintf_s(line,"{\"event\":\"guide_source\",\"transaction\":%llu,\"thread\":%lu,\"depth\":%u,\"begin_qpc\":%lld,\"end_qpc\":%lld,\"level\":\"%s\",\"namespace\":\"%s\",\"member_index\":%d,\"lookups\":%u,\"matches\":%u,\"complete\":%s,\"exceptional\":%s,\"suppressed\":%s,\"expected_known\":%s,\"result\":\"%s\",\"archive\":\"%s\",\"archive_attributed\":%s}\n",
   s.transaction,s.thread,s.depth,s.begin,s.end,s.pathKind==1?"kutnohorsko":s.pathKind==2?"other":"unknown",ktguidesource::Namespaces[s.sourceNamespace],s.index,s.lookups,s.matches,s.complete?"true":"false",s.exceptional?"true":"false",s.suppressed?"true":"false",s.expectedKnown?"true":"false",ktguidesource::Results[static_cast<unsigned>(s.result)],ktguidesource::Archives[s.archive],qualified?"true":"false");writeLine(line);
 }if(stopping.load())break;Sleep(20);}
 sprintf_s(line,"{\"event\":\"stopped\",\"calls\":[%llu,%llu],\"transactions\":%llu,\"ended\":%llu,\"reserved\":%llu,\"committed\":%llu,\"dropped\":%llu,\"unlinked\":%llu,\"overflow\":%llu,\"truncated\":%s,\"write_failed\":%s}\n",calls[0].load(),calls[1].load(),transactions.load(),ended.load(),reserved.load(),committed.load(),dropped.load(),unlinked.load(),overflow.load(),truncated?"true":"false",writeFailed?"true":"false");
 if(!writeFailed)writeLine(line);FlushFileBuffers(logFile);return 0;
}
bool pins(){__try{
 for(const auto& p:ktguidesource::Hooks)if(memcmp(reinterpret_cast<void*>(gameBase+p.rva),p.bytes,16))return false;
 constexpr unsigned char call[]={0xe8,0xd3,0xd6,0xad,0x00},provider[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
 return *reinterpret_cast<std::uintptr_t*>(gameBase+ktguidesource::ProviderVtable+0x248)==gameBase+ktguidesource::ProviderLookup&&!memcmp(reinterpret_cast<void*>(gameBase+ktguidesource::LookupReturn-5),call,5)&&!memcmp(reinterpret_cast<void*>(gameBase+ktguidesource::ProviderLookup),provider,16);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool disable(){bool ok=true;for(void* t:targets)if(t){auto r=MH_DisableHook(t);if(r!=MH_OK&&r!=MH_ERROR_DISABLED)ok=false;}return ok;}
DWORD drainWriter(){for(unsigned i=0;inFlight.load()&&i<5000;++i)Sleep(1);if(inFlight)return ERROR_BUSY;stopping=true;if(WaitForSingleObject(writerThread,5000)!=WAIT_OBJECT_0)return ERROR_BUSY;CloseHandle(writerThread);writerThread=nullptr;CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return 0;}
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
 if(attempted.exchange(true))return ERROR_ALREADY_INITIALIZED;auto error=ktwin::verifyTarget(gameBase);if(error)return error;if(!pins())return ERROR_REVISION_MISMATCH;
 HMODULE pin=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&KtProbeStart),&pin))return GetLastError();
 wchar_t path[32768],logPath[32768];if(!GetModuleFileNameW(ownModule,path,32768))return GetLastError();auto last=wcsrchr(path,L'\\');if(!last)return ERROR_BAD_PATHNAME;*last=0;
 swprintf_s(logPath,L"%s\\guide-source-%lu-%llu.jsonl",path,GetCurrentProcessId(),GetTickCount64());logFile=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(logFile==INVALID_HANDLE_VALUE)return GetLastError();
 if(!writeHeader()){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_WRITE_FAULT;}if(MH_Initialize()!=MH_OK){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 void* cs[]={reinterpret_cast<void*>(&observedSource),reinterpret_cast<void*>(&observedLookup)};
 void** os[]={reinterpret_cast<void**>(&originalSource),reinterpret_cast<void**>(&originalLookup)};
 unsigned created=0;for(;created<2;++created){targets[created]=reinterpret_cast<void*>(gameBase+ktguidesource::Hooks[created].rva);if(MH_CreateHook(targets[created],cs[created],os[created])!=MH_OK)break;}
 if(created!=2){for(unsigned i=0;i<created;++i)MH_RemoveHook(targets[i]);MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 recording=true;writerThread=CreateThread(nullptr,0,writer,nullptr,0,nullptr);if(!writerThread){recording=false;for(void* t:targets)MH_RemoveHook(t);MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 bool enabled=true;for(void* t:targets)if(MH_QueueEnableHook(t)!=MH_OK)enabled=false;if(enabled)enabled=MH_ApplyQueued()==MH_OK;if(!enabled){recording=false;if(disable())drainWriter();return ERROR_INVALID_FUNCTION;}return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*){if(stopBusy.exchange(true))return ERROR_ALREADY_INITIALIZED;if(!writerThread){stopBusy=false;return ERROR_NOT_READY;}recording=false;if(!disable()){stopBusy=false;return ERROR_INVALID_FUNCTION;}const auto result=drainWriter();stopBusy=false;return result;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){ownModule=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
