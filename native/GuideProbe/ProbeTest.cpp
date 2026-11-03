// Owned-process fixtures; no game process, save, installation or API is opened.
#define KT_GUIDE_FIXTURE 1
#include "Probe.cpp"
#include <cstdlib>
#include <thread>
#include <vector>

void check(bool value,const char* message) {if(!value){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
struct Manager {
 alignas(8) unsigned char data[0x60030]{};
 unsigned char ledges[16]{},markers[56]{};
 Manager() {
  *reinterpret_cast<void**>(data)=ledges;*reinterpret_cast<void**>(data+8)=markers;
  *reinterpret_cast<unsigned*>(data+0x10)=1;*reinterpret_cast<unsigned*>(data+0x14)=2;
  const std::uint16_t count=2;memcpy(ledges+6,&count,2);
  const float xyz[6]={1,2,3,0,1,0};memcpy(markers,xyz,24);memcpy(markers+28,xyz,24);markers[24]=1;
 }
};
thread_local unsigned mode=0;
thread_local Manager* testManager=nullptr;
std::atomic<unsigned> forwarded[3]{};
std::atomic<bool> correctArgs{true};
constexpr std::uintptr_t Arg3=0x12345678abcdef10ull,Arg4=0xdeadbeef00000001ull;
__declspec(noinline) void __fastcall targetIndex(void* manager) {
 ++forwarded[2];if(manager!=testManager->data)correctArgs=false;
 if(mode==3)throw 17;if(mode==4)RaiseException(0xe04b7401,0,0,nullptr);
 // A real output mutation supplies the only valid snapshot timing: after host
 // index construction, while typed records are still before ID validation.
 testManager->markers[55]=0xa5;
}
__declspec(noinline) void __fastcall targetBody(void* manager,void* serializer) {
 ++forwarded[1];if(manager!=testManager->data||!serializer)correctArgs=false;
 if(static_cast<unsigned char*>(serializer)[8]==0)return; // Retail save-mode body skips native index.
 ktguide::IndexFn volatile call=&targetIndex;
 if(mode==5)call(nullptr);else call(manager);
}
__declspec(noinline) void __fastcall targetSource(void* owner,const char* path,std::uintptr_t arg3,std::uintptr_t index) {
 ++forwarded[0];if(owner!=testManager||strcmp(path,"Data/Levels/kutnohorsko")||arg3!=Arg3||index!=Arg4)correctArgs=false;
 if(mode==7)return; // Missing XML node: normal source callback performs no load.
 unsigned char serializer[16]{};serializer[8]=mode==2?0:1;
 ktguide::BodyFn volatile call=&targetBody;call(testManager->data,serializer);
 if(mode==6) {const auto old=mode;mode=0;ktguide::SourceFn volatile again=&targetSource;again(owner,path,arg3,index);mode=old;}
}
void reset() {
 Sample s;while(queue.pop(s)){};reserved=0;committed=0;dropped=0;truncated=false;writeFailed=false;recording=true;stopping=false;
 transactions=0;ended=0;overflow=0;unlinked=0;for(auto& c:calls)c=0;
}
Sample run(Manager& manager,unsigned next=0,bool allowed=true) {
 reset();testManager=&manager;mode=next;fixtureCallers=allowed;
 ktguide::SourceFn volatile call=&targetSource;call(&manager,"Data/Levels/kutnohorsko",Arg3,Arg4);
 Sample s;check(queue.pop(s)&&!queue.pop(s),"one outer source observation");
 check(stack.depth==0&&stack.suppressed==0&&inFlight==0,"callback lifetimes drain");return s;
}
bool seh(Manager& manager) {
 __try {observedSource(&manager,"Data/Levels/kutnohorsko",Arg3,Arg4);return false;}
 __except(GetExceptionCode()==0xe04b7401?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
int main() {
 check(KtProbeStart(nullptr)==ERROR_MOD_NOT_FOUND,"absent target refuses");
 check(KtProbeStart(nullptr)==ERROR_ALREADY_INITIALIZED,"no repeated initialization");check(KtProbeStop(nullptr)==ERROR_NOT_READY,"stop absent observer");
 Manager manager;ktguide::Digest d;
 check(fingerprint(manager.data,d)==ktguide::Result::Matched&&d.ledgeCount==1&&d.markerCount==2,"bounded native typed data read");
 check(d.ledges==0xb96a61c0679fa8f7ull&&d.structuralLedges==0x85b1f2e123b0a927ull&&d.markers==0xb8046f51feb018a4ull,
  "independent Python struct/FNV known answers match native typed little-endian streams");
 const auto oldLedges=d.ledges,oldStructure=d.structuralLedges;
 manager.ledges[0]=73;check(fingerprint(manager.data,d)==ktguide::Result::Matched&&d.ledges!=oldLedges&&d.structuralLedges==oldStructure,"entity ID remapping differs from structural data");manager.ledges[0]=0;
 const auto markerHash=d.markers;manager.markers[25]=42;manager.markers[26]=43;manager.markers[27]=44;
 check(fingerprint(manager.data,d)==ktguide::Result::Matched&&d.markers==markerHash,"uninitialized marker padding excluded");
 manager.markers[24]=2;check(fingerprint(manager.data,d)==ktguide::Result::Invalid,"invalid marker flag refused");manager.markers[24]=1;
 *reinterpret_cast<unsigned*>(manager.data+0x14)=65536;check(fingerprint(manager.data,d)==ktguide::Result::Bounds,"hard work bound");*reinterpret_cast<unsigned*>(manager.data+0x14)=2;
 manager.data[0x60028]=1;check(fingerprint(manager.data,d)==ktguide::Result::Editor,"editor state refused");manager.data[0x60028]=0;
 void* protectedMemory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);
 check(protectedMemory&&fingerprint(protectedMemory,d)==ktguide::Result::Unreadable,"guarded invalid read cannot become zero-filled match");
 check(pathKind(static_cast<const char*>(protectedMemory))==0,"path read fault retains unknown");VirtualFree(protectedMemory,0,MEM_RELEASE);
 check(pathKind("secret/path/not-a-level")==2&&pathKind("c:/private/KUTNOHORSKO/")==1,"closed classifications only");
 check(MH_Initialize()==MH_OK,"initialize owned detours");
 void* ts[]={reinterpret_cast<void*>(&targetSource),reinterpret_cast<void*>(&targetBody),reinterpret_cast<void*>(&targetIndex)};
 void* cs[]={reinterpret_cast<void*>(&observedSource),reinterpret_cast<void*>(&observedBody),reinterpret_cast<void*>(&observedIndex)};
 void** os[]={reinterpret_cast<void**>(&originalSource),reinterpret_cast<void**>(&originalBody),reinterpret_cast<void**>(&originalIndex)};
 unsigned char before[3][16];
 for(unsigned i=0;i<3;++i){memcpy(before[i],ts[i],16);check(MH_CreateHook(ts[i],cs[i],os[i])==MH_OK&&MH_QueueEnableHook(ts[i])==MH_OK,"create owned hook");}
 check(MH_ApplyQueued()==MH_OK,"enable3detours");
 const unsigned prior[]={forwarded[0].load(),forwarded[1].load(),forwarded[2].load()};
 const auto sample=run(manager);
 for(unsigned i=0;i<3;++i)check(forwarded[i]==prior[i]+1,"each normal original called exactlyonce");
 check(correctArgs&&sample.complete&&!sample.exceptional&&sample.result==ktguide::Result::Matched&&sample.bodies==1&&sample.indexes==1,"actual3detours preserve opaque registers and exact source/body/index context");
 check(sample.digest.markers==markerHash&&manager.markers[55]==0xa5,"index original effects preserved and padding excluded");
 check(run(manager,0,false).result==ktguide::Result::Caller,"unknown caller never qualifies");
 const auto saving=run(manager,2);check(saving.result==ktguide::Result::Mode&&saving.bodies==1&&saving.indexes==0,"serializer save mode never supplies an index observation");
 const auto absent=run(manager,7);check(absent.result==ktguide::Result::NotObserved&&absent.bodies==0&&absent.indexes==0,"missing source node remains inconclusive");
 check(run(manager,5).result==ktguide::Result::Owner,"foreign manager never borrows callback identity");
 reset();mode=3;testManager=&manager;fixtureCallers=true;bool caught=false;
 try {ktguide::SourceFn volatile call=&targetSource;call(&manager,"Data/Levels/kutnohorsko",Arg3,Arg4);}catch(int value){caught=value==17;}
 Sample exception;check(caught&&queue.pop(exception)&&exception.exceptional&&stack.depth==0&&inFlight==0,"C++ original exception propagated; context released");
 reset();mode=4;check(seh(manager)&&queue.pop(exception)&&exception.exceptional&&stack.depth==0&&inFlight==0,"SEH original exception propagated; context released");
 reset();mode=6;ktguide::SourceFn volatile call=&targetSource;call(&manager,"Data/Levels/kutnohorsko",Arg3,Arg4);
 Sample inner,outer;check(queue.pop(inner)&&queue.pop(outer)&&!queue.pop(exception)&&inner.depth==2&&outer.depth==1,"nested source frames remain independent");
 reset();Frame* frames[ktguide::MaxDepth]{};for(unsigned i=0;i<ktguide::MaxDepth;++i)frames[i]=enter("kutnohorsko",1);
 check(!enter("kutnohorsko",1)&&overflow==1&&!recording,"finite context depth");leave(nullptr,true);for(unsigned i=ktguide::MaxDepth;i>0;--i)leave(frames[i-1],true);
 check(stack.depth==0&&stack.suppressed==0,"overflow contexts drain without borrowing");
 reset();std::vector<std::thread> threads;
 for(unsigned i=0;i<4;++i)threads.emplace_back([]{
  Manager own;testManager=&own;mode=0;fixtureCallers=true;ktguide::SourceFn volatile c=&targetSource;
  for(unsigned j=0;j<30;++j)c(&own,"Data/Levels/kutnohorsko",Arg3,Arg4);
 });
 for(auto& thread:threads)thread.join();unsigned count=0;Sample s;
 while(queue.pop(s)){++count;check(s.result==ktguide::Result::Matched&&s.bodies==1&&s.indexes==1,"parallel identities do not cross");}
 check(count==120&&dropped==0,"parallel capture fully accounted");
 for(unsigned i=0;i<3;++i)check(MH_QueueDisableHook(ts[i])==MH_OK,"disable owned hooks");check(MH_ApplyQueued()==MH_OK,"disable applies");
 for(unsigned i=0;i<3;++i){check(!memcmp(before[i],ts[i],16),"each full original prologue restored");check(MH_RemoveHook(ts[i])==MH_OK,"release owned hook metadata");}
 check(MH_Uninitialize()==MH_OK,"fixture cleanup");
 reset();for(unsigned i=0;i<Capacity+1;++i)enqueue(sample);count=0;while(queue.pop(s))++count;
 check(count==Capacity&&dropped==1&&!recording,"queue full refusal bounded");
 reset();for(unsigned i=0;i<MaxRecords+1;++i){enqueue(sample);check(queue.pop(s)==(i<MaxRecords),"strict session record budget");}
 check(truncated&&!recording&&committed==MaxRecords,"record budget explicit");
 reset();check(!writeLine("{}\n")&&writeFailed&&!recording,"writer fault disables recording");
 // Actual detour-produced sample through production formatter and parser.
 reset();calls[0]=calls[1]=calls[2]=1;transactions=ended=1;
 logFile=CreateFileW(L"guide-conformance.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 check(logFile!=INVALID_HANDLE_VALUE&&writeHeader(),"open owned conformance log");enqueue(sample);stopping=true;writer(nullptr);CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
 puts("GUIDE FIXTURES PASS:3actual detours, opaque arguments, post-index typed digests, padding exclusion, read faults, callers/mode/owner gates, nested/parallel/overflow scopes, exceptions, bounded audit and3site restoration.");
}
