// Owned-process tests. Never touches retail runtime, saves or mounted files.
#define KT_GUIDE_SOURCE_FIXTURE 1
#include "Probe.cpp"
#include <cstdlib>
#include <thread>
#include <vector>
void check(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
alignas(8) std::uintptr_t table[0x250/8]{};
struct File {
 alignas(8) unsigned char data[0x118]{};std::uintptr_t provider=0;
 File(){strcpy_s(reinterpret_cast<char*>(data),260,"Data/Levels/Kutnohorsko/whdata_1");provider=reinterpret_cast<std::uintptr_t>(table);*reinterpret_cast<std::uintptr_t*>(data+0x108)=77;*reinterpret_cast<std::uintptr_t*>(data+0x110)=reinterpret_cast<std::uintptr_t>(&provider);}
};
thread_local unsigned mode=0;thread_local File* testFile=nullptr;
thread_local const char* returned="C:\\Games\\Data\\Levels\\Kutnohorsko\\level.pak";
std::atomic<unsigned> forwarded[2]{};std::atomic<bool> correct{true};
constexpr std::uintptr_t Arg3=0xfedcba9876543210ull,Arg4=0xdeadbeef00000001ull;
__declspec(noinline) const char* __fastcall targetLookup(void* file){
 ++forwarded[1];if(file!=testFile->data)correct=false;
 if(mode==3)throw 17;if(mode==4)RaiseException(0xe04b7401,0,0,nullptr);
 if(mode==5)*reinterpret_cast<std::uintptr_t*>(testFile->data+0x108)=78;
 return returned;
}
__declspec(noinline) void __fastcall targetSource(void* owner,const char* path,std::uintptr_t arg3,std::uintptr_t index){
 ++forwarded[0];if(owner!=testFile||strcmp(path,"data\\levels\\kutnohorsko")||arg3!=Arg3||index!=Arg4)correct=false;
 if(mode==6||mode==7)return; // Missing XML or resource route: no lookup observed.
 ktguidesource::LookupFn volatile lookup=&targetLookup;
 check(lookup(testFile->data)==returned,"exact original pointer returned unchanged");
 if(mode==9)check(lookup(testFile->data)==returned,"second pointer returned unchanged");
 if(mode==10){const auto old=mode;mode=0;ktguidesource::SourceFn volatile nested=&targetSource;nested(owner,path,arg3,index);mode=old;}
}
void reset(){Sample s;while(queue.pop(s)){};reserved=committed=dropped=transactions=ended=overflow=unlinked=0;for(auto& c:calls)c=0;truncated=writeFailed=false;recording=true;stopping=false;}
Sample run(File& file,unsigned m=0,bool allowed=true){
 reset();testFile=&file;mode=m;fixtureCallers=allowed;
 ktguidesource::SourceFn volatile call=&targetSource;call(&file,"data\\levels\\kutnohorsko",Arg3,Arg4);
 Sample s;check(queue.pop(s)&&!queue.pop(s),"one source context retained");check(stack.depth==0&&stack.suppressed==0&&inFlight==0,"scopes released");
 check(stack.frame[0].expected[0]==0,"logical path forgotten after callback");return s;
}
bool seh(File& file){__try{observedSource(&file,"data\\levels\\kutnohorsko",Arg3,Arg4);return false;}__except(GetExceptionCode()==0xe04b7401?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
int main(){
 check(KtProbeStart(nullptr)==ERROR_MOD_NOT_FOUND&&KtProbeStart(nullptr)==ERROR_ALREADY_INITIALIZED&&KtProbeStop(nullptr)==ERROR_NOT_READY,"no target or repeat initialization");
 gameBase=reinterpret_cast<std::uintptr_t>(table)-ktguidesource::ProviderVtable;table[0x248/8]=gameBase+ktguidesource::ProviderLookup;
 char out[260];check(pathCopy("DATA\\levels\\kutnohorsko",out,sizeof out)==PathRead::Ok&&!strcmp(out,"data/levels/kutnohorsko"),"only slash and case normalized");
 check(pathCopy("data/../private",out,sizeof out)==PathRead::Bad&&pathCopy("./data",out,sizeof out)==PathRead::Bad,"dot segments not guessed");
 char longPath[600];memset(longPath,'x',sizeof longPath);check(pathCopy(longPath,out,sizeof out)==PathRead::Truncated,"bounded nonterminated input");
 void* inaccessible=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);check(inaccessible&&pathCopy(static_cast<const char*>(inaccessible),out,sizeof out)==PathRead::Unreadable,"path faults guarded");VirtualFree(inaccessible,0,MEM_RELEASE);
 check(archiveKind("data/levels/kutnohorsko/level.pak")==1&&archiveKind("c:/mods/kuttenberg_thief/data/noop.pak")==2&&archiveKind("c:/other.pak")==3&&archiveKind("")==0,"closed archive classifications");
 check(archiveKind("c:/game/mods/kuttenberg_thief/data/levels/kutnohorsko/level.pak")==2&&archiveKind("c:/game/mods/other/data/levels/kutnohorsko/level.pak")==3,
  "explicit mod archive ownership precedes base-style filename suffix");
 reset();auto* ns=enter("levels/kutnohorsko",1);check(ns&&ns->sample.sourceNamespace==1,"exact levels logical namespace");leave(ns,true);
 reset();ns=enter("c:/private/data/levels/kutnohorsko",1);check(ns&&ns->sample.sourceNamespace==3,"absolute or alias roots never guessed canonical");leave(ns,true);
 check(MH_Initialize()==MH_OK,"owned hooks initialize");void* ts[]={reinterpret_cast<void*>(&targetSource),reinterpret_cast<void*>(&targetLookup)};
 void* cs[]={reinterpret_cast<void*>(&observedSource),reinterpret_cast<void*>(&observedLookup)};void** os[]={reinterpret_cast<void**>(&originalSource),reinterpret_cast<void**>(&originalLookup)};unsigned char originalBytes[2][16];
 for(unsigned i=0;i<2;++i){memcpy(originalBytes[i],ts[i],16);check(MH_CreateHook(ts[i],cs[i],os[i])==MH_OK&&MH_QueueEnableHook(ts[i])==MH_OK,"create owned detours");}check(MH_ApplyQueued()==MH_OK,"enable twohooks");
 File file;const unsigned prior[]={forwarded[0].load(),forwarded[1].load()};const auto observed=run(file);
 check(correct&&observed.complete&&observed.expectedKnown&&observed.sourceNamespace==2&&observed.lookups==1&&observed.matches==1&&observed.result==ktguidesource::Result::Observed&&observed.archive==1,"exact file/source/provider path association");
 for(unsigned i=0;i<2;++i)check(forwarded[i]==prior[i]+1,"each original called exactlyonce");
 check(run(file,0,false).result==ktguidesource::Result::Caller,"unrecognized caller not attributed");
 strcpy_s(reinterpret_cast<char*>(file.data),260,"data/levels/kutnohorsko/whdata_0");check(run(file).result==ktguidesource::Result::MemberMismatch,"other logical file cannot borrow expected source");strcpy_s(reinterpret_cast<char*>(file.data),260,"Data/Levels/Kutnohorsko/whdata_1");
 table[0x248/8]=0;check(run(file).result==ktguidesource::Result::ProviderUnknown,"unknown implementation cannot borrow archive semantics");table[0x248/8]=gameBase+ktguidesource::ProviderLookup;
 const auto provider=file.provider;file.provider+=8;check(run(file).result==ktguidesource::Result::ProviderUnknown,"unknown provider vtable cannot borrow archive semantics");file.provider=provider;
 auto handle=reinterpret_cast<std::uintptr_t*>(file.data+0x108);*handle=0;check(run(file).result==ktguidesource::Result::ProviderUnknown,"missing handle unknown");*handle=77;
 check(run(file,5).result==ktguidesource::Result::Changed&&*handle==78,"host changes preserved and identity change refused");*handle=77;
 returned="";check(run(file).result==ktguidesource::Result::Empty,"empty never means loose file");
 returned=nullptr;check(run(file).result==ktguidesource::Result::Unreadable,"null return preserved and unknown");
 returned=longPath;check(run(file).result==ktguidesource::Result::Truncated,"nonterminated provider path bounded");
 returned="c:/mods/kuttenberg_thief/data/noop.pak";check(run(file).archive==2,"owned archive recognized only after source/provider association");
 returned="C:\\Games\\Data\\Levels\\Kutnohorsko\\level.pak";
 for(unsigned m:{6u,7u}){const auto s=run(file,m);check(s.lookups==0&&s.result==ktguidesource::Result::NotObserved,"missing XML and skipped-provider route unknown");}
 const auto multiple=run(file,9);check(multiple.lookups==2&&multiple.matches==2,"multiple matching observations stay separately counted");
 reset();testFile=&file;fixtureCallers=true;mode=3;bool caught=false;try{ktguidesource::SourceFn volatile call=&targetSource;call(&file,"data\\levels\\kutnohorsko",Arg3,Arg4);}catch(int value){caught=value==17;}
 Sample s;check(caught&&queue.pop(s)&&s.exceptional&&stack.depth==0&&inFlight==0,"original C++ exception propagates and clears context");
 reset();mode=4;check(seh(file)&&queue.pop(s)&&s.exceptional&&stack.depth==0&&inFlight==0,"original SEH propagates and clears context");
 reset();mode=10;ktguidesource::SourceFn volatile source=&targetSource;source(&file,"data\\levels\\kutnohorsko",Arg3,Arg4);Sample a,b;check(queue.pop(a)&&queue.pop(b)&&a.depth==2&&b.depth==1,"nested source contexts independent");
 reset();Frame* frames[ktguidesource::MaxDepth]{};for(unsigned i=0;i<ktguidesource::MaxDepth;++i)frames[i]=enter("data/levels/kutnohorsko",1);check(!enter("data/levels/kutnohorsko",1)&&overflow==1&&!recording,"context bound");leave(nullptr,true);for(unsigned i=ktguidesource::MaxDepth;i>0;--i)leave(frames[i-1],true);
 reset();std::vector<std::thread> threads;for(unsigned i=0;i<4;++i)threads.emplace_back([]{File local;testFile=&local;mode=0;fixtureCallers=true;ktguidesource::SourceFn volatile call=&targetSource;for(unsigned j=0;j<30;++j)call(&local,"data\\levels\\kutnohorsko",Arg3,Arg4);});for(auto& t:threads)t.join();unsigned count=0;while(queue.pop(s)){++count;check(s.result==ktguidesource::Result::Observed&&s.matches==1,"parallel source identity isolated");}check(count==120&&dropped==0,"parallel accounting");
 for(unsigned i=0;i<2;++i)check(MH_QueueDisableHook(ts[i])==MH_OK,"disable hooks");check(MH_ApplyQueued()==MH_OK,"disable applies");for(unsigned i=0;i<2;++i){check(!memcmp(originalBytes[i],ts[i],16),"both full original sites restored");check(MH_RemoveHook(ts[i])==MH_OK,"owned metadata removed");}check(MH_Uninitialize()==MH_OK,"owned hooks clean");
 reset();for(unsigned i=0;i<Capacity+1;++i)enqueue(observed);count=0;while(queue.pop(s))++count;check(count==Capacity&&dropped==1&&!recording,"queue full stops bounded capture");
 reset();for(unsigned i=0;i<MaxRecords+1;++i){enqueue(observed);check(queue.pop(s)==(i<MaxRecords),"record limit");}check(truncated&&!recording&&committed==MaxRecords,"limit explicit");
 reset();check(!writeLine("{}\n")&&writeFailed&&!recording,"disk failure disables audit");
 reset();calls[0]=calls[1]=1;transactions=ended=1;logFile=CreateFileW(L"guide-source-conformance.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);check(logFile!=INVALID_HANDLE_VALUE&&writeHeader(),"conformance opens");enqueue(observed);stopping=true;writer(nullptr);CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
 puts("GUIDE SOURCE FIXTURES PASS: twoactualdetours, exactarguments and returnedpointer, source/file/provider association, empty/skipped/mismatch unknowns, readfaults, nested/parallelbounds, exceptions and2site restoration.");
}
