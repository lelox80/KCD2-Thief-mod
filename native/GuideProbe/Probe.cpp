// Observation only: forward normal source/load/index callbacks, never load,
// register, insert, modify guides or invoke traversal ourselves.
#include "../StateBridge/WindowsTarget.h"
#include "../StateBridge/AuditQueue.h"
#include "Guide.h"
#include "MinHook.h"
#include <intrin.h>
#include <cmath>
#include <cwchar>

namespace {
ktguide::SourceFn originalSource=nullptr;
ktguide::BodyFn originalBody=nullptr;
ktguide::IndexFn originalIndex=nullptr;
std::uintptr_t gameBase=0;HMODULE ownModule=nullptr;void* targets[3]{};
HANDLE writerThread=nullptr,logFile=INVALID_HANDLE_VALUE;
std::atomic<bool> attempted{false},recording{false},stopping{false},stopBusy{false},writeFailed{false},truncated{false};
std::atomic<unsigned> inFlight{0};
std::atomic<unsigned long long> calls[3]{},transactions{0},ended{0},dropped{0},reserved{0},committed{0},unlinked{0},overflow{0};
constexpr unsigned Capacity=256,MaxRecords=1024;
struct Sample {
 std::uint64_t transaction=0;long long begin=0,end=0;DWORD thread=0;
 unsigned depth=0,pathKind=0,index=0,bodies=0,indexes=0;bool complete=false,exceptional=false,suppressed=false;
 ktguide::Result result=ktguide::Result::NotObserved;ktguide::Digest digest;
};
struct Frame {Sample sample;void* manager=nullptr;unsigned bodyDepth=0;};
struct Stack {unsigned depth=0,suppressed=0;Frame frame[ktguide::MaxDepth];};
thread_local Stack stack;
using Queue=kt::AuditQueue<Sample,Capacity>;Queue queue;
#ifdef KT_GUIDE_FIXTURE
thread_local bool fixtureCallers=false;
#endif
long long ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
bool caller(std::uintptr_t address,std::uintptr_t expected) {
#ifdef KT_GUIDE_FIXTURE
 if(fixtureCallers)return true;
#endif
 return address==gameBase+expected;
}
// Read at most256 bytes and retain only a closed level classification. Never
// store a caller-supplied path, arbitrary string, pointer or entity ID in logs.
unsigned pathKind(const char* path) {
 __try {
  char p[256];if(!path)return 0;unsigned length=0;
  for(;length<sizeof p;++length) {const char c=path[length];if(!c)break;
   p[length]=c=='\\'?'/':c>='A'&&c<='Z'?static_cast<char>(c+32):c;}
  if(length==sizeof p)return 0;while(length&&p[length-1]=='/')--length;
  constexpr char suffix[]="kutnohorsko";constexpr unsigned n=sizeof suffix-1;
  if(length>=n&&!memcmp(p+length-n,suffix,n)&&(length==n||p[length-n-1]=='/'))return 1;
  return 2;
 } __except(EXCEPTION_EXECUTE_HANDLER) {return 0;}
}
ktguide::Result fingerprint(void* manager,ktguide::Digest& out) {
 __try {
  if(!manager)return ktguide::Result::Unreadable;
  const auto m=static_cast<unsigned char*>(manager);
  if(m[0x60028])return ktguide::Result::Editor;
  auto ledges=*reinterpret_cast<const unsigned char* const*>(m);
  auto markers=*reinterpret_cast<const unsigned char* const*>(m+8);
  const auto lc=*reinterpret_cast<const std::uint32_t*>(m+0x10),mc=*reinterpret_cast<const std::uint32_t*>(m+0x14);
  if(lc>ktguide::MaxElements||mc>ktguide::MaxElements||(!ledges&&lc)||(!markers&&mc))return ktguide::Result::Bounds;
  ktguide::Digest value;value.ledgeCount=lc;value.markerCount=mc;
  for(unsigned i=0;i<lc;++i) {
   unsigned char bytes[16];memcpy(bytes,ledges+i*16,16);
   std::uint16_t first=0,count=0;float adjust=0;
   memcpy(&first,bytes+4,2);memcpy(&count,bytes+6,2);memcpy(&adjust,bytes+12,4);
   if(first>mc||count>mc-first||!std::isfinite(adjust))return ktguide::Result::Invalid;
   for(auto b:bytes)ktguide::hashByte(value.ledges,b);
   for(unsigned j=4;j<16;++j)ktguide::hashByte(value.structuralLedges,bytes[j]);
  }
  for(unsigned i=0;i<mc;++i) {
   unsigned char bytes[25];memcpy(bytes,markers+i*28,25);
   for(unsigned j=0;j<6;++j) {float f=0;memcpy(&f,bytes+j*4,4);if(!std::isfinite(f))return ktguide::Result::Invalid;}
   if(bytes[24]>1)return ktguide::Result::Invalid;
   for(auto b:bytes)ktguide::hashByte(value.markers,b);
  }
  if(*reinterpret_cast<const unsigned char* const*>(m)!=ledges||*reinterpret_cast<const unsigned char* const*>(m+8)!=markers||
   *reinterpret_cast<const std::uint32_t*>(m+0x10)!=lc||*reinterpret_cast<const std::uint32_t*>(m+0x14)!=mc||m[0x60028])return ktguide::Result::Changed;
  out=value;return ktguide::Result::Matched;
 } __except(EXCEPTION_EXECUTE_HANDLER) {return ktguide::Result::Unreadable;}
}
bool readMode(void* serializer,bool& mode) {
 __try {if(!serializer)return false;const auto b=static_cast<unsigned char*>(serializer)[8];if(b>1)return false;mode=b==1;return true;}
 __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void enqueue(const Sample& sample) {
 auto n=reserved.load();
 do {if(n>=MaxRecords) {truncated=true;recording=false;return;}}
 while(!reserved.compare_exchange_weak(n,n+1));
 Queue::Ticket t;
 if(queue.reserve(t)!=Queue::Result::Reserved) {++dropped;recording=false;return;}
 queue.commit(t,sample);++committed;
}
Frame* enter(const char* path,std::uintptr_t index) {
 if(!recording.load()||stack.suppressed) {++stack.suppressed;return nullptr;}
 if(stack.depth==ktguide::MaxDepth) {stack.frame[stack.depth-1].sample.suppressed=true;++overflow;recording=false;++stack.suppressed;return nullptr;}
 auto& f=stack.frame[stack.depth++];f=Frame{};f.sample.transaction=++transactions;f.sample.begin=ticks();
 f.sample.thread=GetCurrentThreadId();f.sample.depth=stack.depth;f.sample.pathKind=pathKind(path);
 f.sample.index=static_cast<unsigned>(index);return &f;
}
void leave(Frame* frame,bool complete) {
 if(!frame) {if(stack.suppressed)--stack.suppressed;return;}
 frame->sample.complete=complete;frame->sample.exceptional=!complete;frame->sample.end=ticks();
 frame->sample.suppressed=frame->sample.suppressed||stack.suppressed!=0;
 enqueue(frame->sample);++ended;--stack.depth;
}
void __fastcall observedSource(void* a,const char* path,std::uintptr_t c,std::uintptr_t index) {
 ++inFlight;++calls[0];Frame* frame=enter(path,index);bool complete=false;
 __try {originalSource(a,path,c,index);complete=true;}
 __finally {leave(frame,complete);--inFlight;}
}
void __fastcall observedBody(void* manager,void* serializer) {
 ++inFlight;++calls[1];Frame* f=stack.depth&&!stack.suppressed?&stack.frame[stack.depth-1]:nullptr;
 bool linked=false;
 if(f) {
  ++f->sample.bodies;
  if(!caller(reinterpret_cast<std::uintptr_t>(_ReturnAddress()),ktguide::BodyReturn))f->sample.result=ktguide::Result::Caller;
  else if(f->bodyDepth) {f->sample.suppressed=true;f->sample.result=ktguide::Result::Owner;}
  else {bool mode=false;
   if(!readMode(serializer,mode))f->sample.result=ktguide::Result::Unreadable;
   else if(!mode)f->sample.result=ktguide::Result::Mode;
   else {f->manager=manager;++f->bodyDepth;linked=true;}
  }
 } else ++unlinked;
 __try {originalBody(manager,serializer);}
 __finally {if(linked) {--f->bodyDepth;f->manager=nullptr;}--inFlight;}
}
void __fastcall observedIndex(void* manager) {
 ++inFlight;++calls[2];Frame* f=stack.depth&&!stack.suppressed?&stack.frame[stack.depth-1]:nullptr;
 bool linked=false;
 if(f) {++f->sample.indexes;
  if(!caller(reinterpret_cast<std::uintptr_t>(_ReturnAddress()),ktguide::IndexReturn))f->sample.result=ktguide::Result::Caller;
  else if(!f->bodyDepth||f->manager!=manager)f->sample.result=ktguide::Result::Owner;
  else linked=true;
 } else ++unlinked;
 __try {originalIndex(manager);if(linked)f->sample.result=fingerprint(manager,f->sample.digest);}
 __finally {--inFlight;}
}
bool writeLine(const char* line) {
 DWORD written=0;const auto length=static_cast<DWORD>(strlen(line));
 if(!WriteFile(logFile,line,length,&written,nullptr)||written!=length) {writeFailed=true;recording=false;return false;}return true;
}
bool writeHeader() {
 return writeLine("{\"event\":\"started\",\"probe\":\"guide-load\",\"protocol\":1,\"motion_compiled\":false,\"registers_guides\":false,\"record_limit\":1024,\"max_elements\":65535,\"fingerprint\":\"fnv1a64-typed-le\",\"archive_source_verified\":false}\n");
}
DWORD WINAPI writer(void*) {
 char line[2048];Sample s;
 for(;;) {
  while(queue.pop(s)) {
   const bool qualified=s.complete&&!s.exceptional&&!s.suppressed&&s.bodies==1&&s.indexes==1&&s.result==ktguide::Result::Matched;
   sprintf_s(line,"{\"event\":\"guide_load\",\"transaction\":%llu,\"thread\":%lu,\"depth\":%u,\"begin_qpc\":%lld,\"end_qpc\":%lld,\"level\":\"%s\",\"member_index\":%u,\"bodies\":%u,\"indexes\":%u,\"complete\":%s,\"exceptional\":%s,\"suppressed\":%s,\"result\":\"%s\",\"registry_observed\":%s,\"ledge_count\":%u,\"marker_count\":%u,\"ledge_fnv1a64\":\"%016llx\",\"structural_ledge_fnv1a64\":\"%016llx\",\"marker_fnv1a64\":\"%016llx\"}\n",
    s.transaction,s.thread,s.depth,s.begin,s.end,s.pathKind==1?"kutnohorsko":s.pathKind==2?"other":"unknown",s.index,s.bodies,s.indexes,
    s.complete?"true":"false",s.exceptional?"true":"false",s.suppressed?"true":"false",ktguide::Results[static_cast<unsigned>(s.result)],qualified?"true":"false",
    s.digest.ledgeCount,s.digest.markerCount,s.digest.ledges,s.digest.structuralLedges,s.digest.markers);writeLine(line);
  }
  if(stopping.load())break;Sleep(20);
 }
 sprintf_s(line,"{\"event\":\"stopped\",\"calls\":[%llu,%llu,%llu],\"transactions\":%llu,\"ended\":%llu,\"reserved\":%llu,\"committed\":%llu,\"dropped\":%llu,\"unlinked\":%llu,\"overflow\":%llu,\"truncated\":%s,\"write_failed\":%s}\n",
  calls[0].load(),calls[1].load(),calls[2].load(),transactions.load(),ended.load(),reserved.load(),committed.load(),dropped.load(),unlinked.load(),overflow.load(),truncated?"true":"false",writeFailed?"true":"false");
 if(!writeFailed)writeLine(line);FlushFileBuffers(logFile);return 0;
}
bool pins() {
 __try {
  for(const auto& pin:ktguide::Hooks)if(memcmp(reinterpret_cast<void*>(gameBase+pin.rva),pin.bytes,16))return false;
  constexpr unsigned char body[]={0xe8,0x1c,0,0,0},index[]={0xe8,0xa2,0x57,0x4b,0xff};
  return !memcmp(reinterpret_cast<void*>(gameBase+ktguide::BodyReturn-5),body,5)&&!memcmp(reinterpret_cast<void*>(gameBase+ktguide::IndexReturn-5),index,5);
 } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool disable() {bool ok=true;for(void* t:targets)if(t){auto r=MH_DisableHook(t);if(r!=MH_OK&&r!=MH_ERROR_DISABLED)ok=false;}return ok;}
DWORD drainWriter() {
 for(unsigned i=0;inFlight.load()&&i<5000;++i)Sleep(1);if(inFlight)return ERROR_BUSY;
 stopping=true;if(WaitForSingleObject(writerThread,5000)!=WAIT_OBJECT_0)return ERROR_BUSY;
 CloseHandle(writerThread);writerThread=nullptr;CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return 0;
}
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
 if(attempted.exchange(true))return ERROR_ALREADY_INITIALIZED;
 auto error=ktwin::verifyTarget(gameBase);if(error)return error;if(!pins())return ERROR_REVISION_MISMATCH;
 HMODULE pin=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&KtProbeStart),&pin))return GetLastError();
 wchar_t path[32768],logPath[32768];if(!GetModuleFileNameW(ownModule,path,32768))return GetLastError();auto last=wcsrchr(path,L'\\');if(!last)return ERROR_BAD_PATHNAME;*last=0;
 swprintf_s(logPath,L"%s\\guide-load-%lu-%llu.jsonl",path,GetCurrentProcessId(),GetTickCount64());
 logFile=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(logFile==INVALID_HANDLE_VALUE)return GetLastError();
 if(!writeHeader()){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_WRITE_FAULT;}
 if(MH_Initialize()!=MH_OK){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 void* callbacks[]={reinterpret_cast<void*>(&observedSource),reinterpret_cast<void*>(&observedBody),reinterpret_cast<void*>(&observedIndex)};
 void** originals[]={reinterpret_cast<void**>(&originalSource),reinterpret_cast<void**>(&originalBody),reinterpret_cast<void**>(&originalIndex)};
 unsigned created=0;for(;created<3;++created){targets[created]=reinterpret_cast<void*>(gameBase+ktguide::Hooks[created].rva);if(MH_CreateHook(targets[created],callbacks[created],originals[created])!=MH_OK)break;}
 if(created!=3){for(unsigned i=0;i<created;++i)MH_RemoveHook(targets[i]);MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 recording=true;writerThread=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
 if(!writerThread){recording=false;for(void* t:targets)MH_RemoveHook(t);MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
 bool enabled=true;for(void* t:targets)if(MH_QueueEnableHook(t)!=MH_OK)enabled=false;if(enabled)enabled=MH_ApplyQueued()==MH_OK;
 if(!enabled){recording=false;if(disable())drainWriter();return ERROR_INVALID_FUNCTION;}return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
 if(stopBusy.exchange(true))return ERROR_ALREADY_INITIALIZED;if(!writerThread){stopBusy=false;return ERROR_NOT_READY;}
 recording=false;if(!disable()){stopBusy=false;return ERROR_INVALID_FUNCTION;}const auto result=drainWriter();stopBusy=false;return result;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {if(reason==DLL_PROCESS_ATTACH){ownModule=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
