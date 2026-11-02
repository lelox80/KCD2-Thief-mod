// Owned-process fixtures only. No game process or retail module is opened.
#ifndef KT_ALLOW_MOTION
#define KT_ALLOW_MOTION 0
#endif
#include "Bridge.cpp"
#include <cstdlib>
#include <array>
void check(bool ok,const char* why) {if(!ok) {fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
std::atomic<unsigned> forwarded{0};
__declspec(noinline) bool __fastcall fixtureRequest(void* controller,void* request) {
    ++forwarded;std::uint64_t flags=0;kt::Vec3 v;
    bool ok=readRequest(request,flags,v);
    return ok&&controller==request&&flags==0x1060&&v.y==4;
}
bool __fastcall fixtureUnwind(void*,void*) {RaiseException(0xe04b5401,0,0,nullptr);return false;}
unsigned unsafeApiCalls=0;
int __fastcall fixtureBadLua(LState*,int) {++unsafeApiCalls;RaiseException(0xe04b5402,0,0,nullptr);return 0;}
std::uintptr_t __fastcall fixtureUpdate(void*) {return 1234;}
bool testUnwind(void* request,Audit& a,Journal::Ticket ticket) {
    __try {finishRequest(request,request,a,true,true,ticket,cancellationGeneration.load());}
    __except(EXCEPTION_EXECUTE_HANDLER) {return true;}
    return false;
}
bool testPublisherUnwind(LState* L) {
    __try {publish(L);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return true;}
    return false;
}
int main() {
    check(KtSwoopStart(nullptr)==ERROR_MOD_NOT_FOUND,"no-game startup refusal");
    check(KtSwoopStart(nullptr)==ERROR_ALREADY_INITIALIZED,"one start per process");
    check(KtSwoopStop(nullptr)==ERROR_NOT_READY,"stop before start");
    alignas(8) unsigned char fakeLua[0x50]{},stack[16*16]{};
    auto base=reinterpret_cast<std::uintptr_t>(stack);
    memcpy(fakeLua+0x18,&base,8);auto top=base+16*5;memcpy(fakeLua+0x10,&top,8);
    int height=0;check(stackHeight(fakeLua,height)&&height==5,"verified stack index read");
    check(shrinkStack(fakeLua,3)&&stackHeight(fakeLua,height)&&height==3,"stack shrink only");
    check(!shrinkStack(fakeLua,4),"never grow by pointer write");
    alignas(8) unsigned char c[16]{},actor[0x188]{},entity[0x18]{};
    auto putPtr=[](unsigned char* p,std::uintptr_t n){memcpy(p,&n,8);};
    putPtr(c,ktretail::ControllerVtable);putPtr(c+8,reinterpret_cast<std::uintptr_t>(actor));
    putPtr(actor,ktretail::PlayerVtable);putPtr(actor+0x180,reinterpret_cast<std::uintptr_t>(c));
    putPtr(actor+0x38,reinterpret_cast<std::uintptr_t>(entity));putPtr(entity,ktretail::EntityVtable);
    std::uint32_t id=42;memcpy(actor+0x30,&id,4);memcpy(entity+0xc,&id,4);
    std::uint64_t token=0;std::uint32_t observed=0;
    check(ktwin::owner(0,c,observed,token)&&observed==42&&token==reinterpret_cast<std::uintptr_t>(actor),"native owner triple identity");
    id=43;memcpy(entity+0xc,&id,4);check(!ktwin::owner(0,c,observed,token),"mismatched owner refuses");
    check(!ktwin::owner(0,nullptr,observed,token),"missing owner refuses");
    unsigned char request[0x1b0]{};std::uint64_t flags=0x1060;float x=1,y=2,z=3;
    memcpy(request,&flags,8);memcpy(request+0x7c,&x,4);memcpy(request+0x80,&y,4);memcpy(request+0x84,&z,4);
    std::array<unsigned char,sizeof request> before{};memcpy(before.data(),request,sizeof request);
    originalRequest=&fixtureRequest;
    running=true;fatal=false;forceBaseline=false;bool changed=false;
    check(forwardXY(request,request,2,4,x,y,cancellationGeneration.load(),changed)&&changed,"original controller receives the temporary bounded xy");
    check(!memcmp(before.data(),request,sizeof request),"original entire request restored after call");
    running=false;
    check(!forwardXY(request,request,2,4,x,y,cancellationGeneration.load(),changed)&&!changed,"stop rechecked at mutation point");
    running=true;auto priorGeneration=cancellationGeneration.load();++cancellationGeneration;
    check(!forwardXY(request,request,2,4,x,y,priorGeneration,changed)&&!changed,"new cancellation generation rejects cached permission");
    running=false;
    check(MH_Initialize()==MH_OK,"fixture MinHook initialization");
    check(MH_CreateHook(&fixtureRequest,&movement,reinterpret_cast<void**>(&originalRequest))==MH_OK&&MH_EnableHook(&fixtureRequest)==MH_OK,"actual hook on owned function");
    RequestFn volatile invoke=&fixtureRequest;
    check(!invoke(request,request)&&forwarded==4,"observe-only hook forwards original unmodified input/result");
    check(MH_DisableHook(&fixtureRequest)==MH_OK&&MH_RemoveHook(&fixtureRequest)==MH_OK&&MH_Uninitialize()==MH_OK,"fixture restoration");
    originalRequest=&fixtureRequest; // The removed fixture trampoline is no longer callable.
    LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);qpcFrequency=static_cast<double>(freq.QuadPart);
    Audit a;a.original={1,2,3};check(enqueue(a),"bounded audit queue");Audit b;check(pop(b)&&b.original.y==2,"audit queue preserves sample");
    Journal::Ticket reserved;check(reserveAudit(reserved),"reserve audit before owned exceptional host call");
    a.original={x,y,z};a.wouldApply=true;a.snapshot.packet.dryRun=false;a.decision.motion.request={2,4,3};
    running=true;forceBaseline=false;originalRequest=&fixtureUnwind;
    check(TryAcquireSRWLockExclusive(&coreLock)!=0,"owned action serialization");++inFlight;
    check(testUnwind(request,a,reserved),"host exception propagates to owned caller");
    check(inFlight==0&&pop(b)&&b.wrote==(KT_ALLOW_MOTION!=0),"exception commits actual audit and releases flight count");
    check(!memcmp(before.data(),request,sizeof request),"exception restores original entire host request");
    check(TryAcquireSRWLockExclusive(&coreLock)!=0,"exception releases action serialization");ReleaseSRWLockExclusive(&coreLock);
    originalRequest=&fixtureRequest;a=Audit{};
    for(unsigned i=0;i<Capacity;++i) check(enqueue(a),"fill audit queue with owned records");
    check(!enqueue(a)&&fatal&&dropped==1&&auditFault==AuditFault::QueueFull,"real queue exhaustion is explicit fail-closed");
    running=true;
    check(!forwardXY(request,request,2,4,x,y,cancellationGeneration.load(),changed)&&!changed,"queue failure forbids host mutation");
    check(!memcmp(before.data(),request,sizeof request),"queue failure forwards unchanged entire request");
    while(pop(b)) {}
    fatal=false;auditFault=AuditFault::None;auditReserved=MaxRecords-1;
    check(enqueue(a),"last bounded record available");
    check(!enqueue(a)&&fatal&&auditFault==AuditFault::CaptureLimit,"budget reserved before mutation");
    check(!forwardXY(request,request,2,4,x,y,cancellationGeneration.load(),changed)&&!changed,"capture limit forbids host mutation");
    check(pop(b)&&b.kind==0&&pop(b)&&b.kind==3&&!pop(b),"budget tombstone drains without logging host action");
    fatal=false;auditFault=AuditFault::None;logFile=INVALID_HANDLE_VALUE;
    writeLine("owned fixture: invalid handle");
    check(fatal&&auditFault==AuditFault::WriteFailure,"writer failure explicitly latches motion off");
    void* fakeSystem[3]{};fakeSystem[2]=fakeLua;
    lua.checkstack=&fixtureBadLua;fatal=false;luaUnsafe=false;
    check(!protectedUpdate(fakeSystem)&&fatal&&luaUnsafe&&unsafeApiCalls==1,"Lua API exception latches VM unsafe");
    originalUpdate=&fixtureUpdate;scriptThread=GetCurrentThreadId();running=true;
    check(scriptUpdate(fakeSystem)==1234&&unsafeApiCalls==1,"fault reporting never re-enters an unsafe VM");
    luaUnsafe=false;fatal=false;registeredState=fakeLua;lua.type=&fixtureBadLua;
    check(testPublisherUnwind(fakeLua)&&inFlight==0,"publisher API unwind releases flight count for Stop");
    check(!restoreStack(fakeLua,4)&&fatal&&luaUnsafe,"every failed stack restoration latches VM unsafe");
    running=false;
    puts("BRIDGE OWNED FIXTURES PASS: absent target, stack restoration, owner checks, temporary xy forwarding/restoration and observe-only detour.");
}
