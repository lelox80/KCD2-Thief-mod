// Build-pinned bridge plus bounded controller-request prototype.
// KT_ALLOW_MOTION=0 is the first retail test: every request is forwarded intact.
#include "WindowsTarget.h"
#include "Adapter.h"
#include "AuditQueue.h"
#include "MinHook.h"
#include <atomic>
#include <intrin.h>
#include <cwchar>

#ifndef KT_ALLOW_MOTION
#define KT_ALLOW_MOTION 0
#endif

namespace {
using LState=void;
using CFunction=int (__fastcall *)(LState*);
struct LuaApi {
    void (__fastcall *getfield)(LState*,int,const char*)=nullptr;
    void (__fastcall *setfield)(LState*,int,const char*)=nullptr;
    int (__fastcall *type)(LState*,int)=nullptr;
    const char* (__fastcall *tolstring)(LState*,int,std::size_t*)=nullptr;
    void* (__fastcall *touserdata)(LState*,int)=nullptr;
    void (__fastcall *pushcclosure)(LState*,CFunction,int)=nullptr;
    int (__fastcall *pcall)(LState*,int,int,int)=nullptr;
    int (__fastcall *checkstack)(LState*,int)=nullptr;
    void bind(std::uintptr_t b) {
        getfield=reinterpret_cast<decltype(getfield)>(b+0xad496c);
        setfield=reinterpret_cast<decltype(setfield)>(b+0xa26d60);
        type=reinterpret_cast<decltype(type)>(b+0x715e00);
        tolstring=reinterpret_cast<decltype(tolstring)>(b+0x718a18);
        touserdata=reinterpret_cast<decltype(touserdata)>(b+0x71805c);
        pushcclosure=reinterpret_cast<decltype(pushcclosure)>(b+0xb9663c);
        pcall=reinterpret_cast<decltype(pcall)>(b+0x713914);
        checkstack=reinterpret_cast<decltype(checkstack)>(b+0xb96168);
    }
} lua;
using RequestFn=bool (__fastcall *)(void*,void*);
using UpdateFn=std::uintptr_t (__fastcall *)(void*);
RequestFn originalRequest=nullptr;UpdateFn originalUpdate=nullptr;
std::uintptr_t gameBase=0;HMODULE ownModule=nullptr;
HANDLE logFile=INVALID_HANDLE_VALUE,writerHandle=nullptr;
void* requestTarget=nullptr;void* updateTarget=nullptr;
double qpcFrequency=0;
std::atomic<bool> attempted{false},running{false},stopping{false},stopBusy{false},fatal{false},forceBaseline{false};
std::atomic<bool> luaUnsafe{false};
std::atomic<unsigned> inFlight{0};std::atomic<DWORD> scriptThread{0};
std::atomic<unsigned long long> cancellationGeneration{1};
unsigned long long appliedBaseline=0;
std::atomic<unsigned long long> calls{0},playerCalls{0},published{0},publishErrors{0},dropped{0},wouldModify{0},actualModify{0};
SRWLOCK snapshotLock=SRWLOCK_INIT,coreLock=SRWLOCK_INIT;
kt::NativeSnapshot latest;kt::MovementAdapter adapter;
LState* registeredState=nullptr;bool callbackRegistered=false;
bool faultReported=false;
struct Audit {
    unsigned kind=0;DWORD thread=0;long long ticks=0;
    kt::NativeSnapshot snapshot;
    std::uint64_t flags=0;std::uint32_t owner=0;
    kt::AdapterResult decision;
    kt::Vec3 original;
    bool wrote=false;
    bool wouldApply=false;
    char error[64]{};
};
constexpr unsigned Capacity=2048,MaxRecords=100000;
using Journal=kt::AuditQueue<Audit,Capacity>;
Journal auditQueue;
std::atomic<unsigned long long> auditReserved{0},auditBusy{0},requestContentions{0};
enum class AuditFault : unsigned {None,QueueFull,CaptureLimit,WriteFailure};
std::atomic<AuditFault> auditFault{AuditFault::None};
using SampleKey=std::array<std::uint64_t,15>;
kt::AuditSampler<SampleKey> stateSampler,requestSampler;
void auditFailure(AuditFault reason) {
    AuditFault expected=AuditFault::None;auditFault.compare_exchange_strong(expected,reason);fatal=true;
}
SampleKey stateKey(const kt::NativeSnapshot& s) {
    const auto& p=s.packet;
    return {p.epoch,p.generation,p.identity,p.entity,p.knownMask,p.safeMask,p.requestSerial,p.resetSerial,
            p.enabled,p.dryRun,s.valid,0,0,0,0};
}
SampleKey requestKey(const Audit& a) {
    auto key=stateKey(a.snapshot);
    key[10]=a.flags;key[11]=static_cast<unsigned>(a.decision.reason);
    key[12]=static_cast<unsigned>(a.decision.motion.reason);
    key[13]=a.decision.eligible;key[14]=a.decision.identityMatch;return key;
}

long long ticks() {LARGE_INTEGER n;QueryPerformanceCounter(&n);return n.QuadPart;}
double seconds(long long t) {return static_cast<double>(t)/qpcFrequency;}
bool reserveAudit(Journal::Ticket& ticket) {
    if(fatal.load()) return false;
    const auto result=auditQueue.reserve(ticket);
    if(result==Journal::Result::Busy) {++auditBusy;return false;}
    if(result==Journal::Result::Full) {++dropped;auditFailure(AuditFault::QueueFull);return false;}
    if(auditReserved.fetch_add(1)>=MaxRecords) {
        Audit tombstone;tombstone.kind=3;auditQueue.commit(ticket,tombstone);
        auditFailure(AuditFault::CaptureLimit);return false;
    }
    return true;
}
bool enqueue(const Audit& a) {
    Journal::Ticket ticket;if(!reserveAudit(ticket)) return false;
    auditQueue.commit(ticket,a);return true;
}
bool pop(Audit& a) {return auditQueue.pop(a);}
void invalidate() {
    AcquireSRWLockExclusive(&snapshotLock);latest.valid=false;ReleaseSRWLockExclusive(&snapshotLock);
    forceBaseline=true;
    ++cancellationGeneration;
}
void errorAudit(const char* reason) {
    Audit a;a.kind=2;a.thread=GetCurrentThreadId();a.ticks=ticks();strcpy_s(a.error,reason);enqueue(a);
}
bool copySnapshot(kt::NativeSnapshot& s) {
    if(!TryAcquireSRWLockShared(&snapshotLock)) return false;
    s=latest;ReleaseSRWLockShared(&snapshotLock);return true;
}
// Verified retail Lua stack layout; only scalar heights survive engine calls.
// Recompute from CURRENT base after possible engine-owned stack reallocation.
bool stackHeight(LState* L,int& height) {
    __try {
        auto address=reinterpret_cast<std::uintptr_t>(L);
        auto top=*reinterpret_cast<std::uintptr_t*>(address+0x10);
        auto base=*reinterpret_cast<std::uintptr_t*>(address+0x18);
        if(top<base||(top-base)%16||(top-base)/16>4096) return false;
        height=static_cast<int>((top-base)/16);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool shrinkStack(LState* L,int height) {
    __try {
        auto address=reinterpret_cast<std::uintptr_t>(L);
        auto top=*reinterpret_cast<std::uintptr_t*>(address+0x10);
        auto base=*reinterpret_cast<std::uintptr_t*>(address+0x18);
        if(height<0||height>4096||top<base||(top-base)%16||static_cast<std::uintptr_t>(height)>(top-base)/16) return false;
        *reinterpret_cast<std::uintptr_t*>(address+0x10)=base+static_cast<std::uintptr_t>(height)*16;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool restoreStack(LState* L,int height) {
    if(shrinkStack(L,height)) return true;
    luaUnsafe=true;fatal=true;return false;
}
LState* readState(void* system) {
    __try {return *reinterpret_cast<LState**>(static_cast<unsigned char*>(system)+0x10);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
int publishImpl(LState* L) {
    // This closure remains pinned and inert after Stop. No off-thread cleanup.
    if(!running.load()||fatal.load()) return 0;
    if(GetCurrentThreadId()!=scriptThread.load()||L!=registeredState) {
        if(publishErrors.fetch_add(1)==0) errorAudit("publish_thread_or_state");luaUnsafe=true;fatal=true;invalidate();return 0;
    }
    if(lua.type(L,1)!=4) {if(publishErrors.fetch_add(1)==0) errorAudit("publish_packet_type");invalidate();return 0;}
    std::size_t length=0;auto bytes=lua.tolstring(L,1,&length);
    kt::BridgePacket packet;
    if(!bytes||!kt::parsePacket(std::string_view(bytes,length),packet)) {if(publishErrors.fetch_add(1)==0) errorAudit("publish_packet_schema");invalidate();return 0;}
    int idType=lua.type(L,2);
    if(idType==2) {
        auto handle=reinterpret_cast<std::uintptr_t>(lua.touserdata(L,2));
        if(!handle||handle>0xffffffffull||(packet.entity&&packet.entity!=handle)) {
            packet.entity=0;packet.safeMask&=~4u;
        } else packet.entity=static_cast<std::uint32_t>(handle);
    } else {
        // Only the actual typed engine handle is identity evidence. A numeric
        // packet field alone is never sufficient to acquire the actor.
        packet.entity=0;packet.safeMask&=~4u;
    }
    const long long t=ticks();const double now=seconds(t);
    kt::NativeSnapshot s;s.packet=packet;s.wallstamp=now;s.valid=true;
    bool log=false;
    AcquireSRWLockExclusive(&snapshotLock);
    const auto& prior=latest.packet;
    bool same=latest.valid&&prior.epoch==packet.epoch&&prior.generation==packet.generation&&prior.identity==packet.identity;
    if(same&&(packet.sequence<=prior.sequence||packet.observerSequence<prior.observerSequence||packet.asyncSeconds<prior.asyncSeconds)) {
        s.valid=false;++publishErrors;forceBaseline=true;
    }
    if(same&&packet.observerSequence&&packet.observerSequence==prior.observerSequence)
        s.observerWallstamp=latest.observerWallstamp;
    else if(packet.observerSequence&&packet.observerAge>=0&&packet.observerAge<=1)
        s.observerWallstamp=now-packet.observerAge;
    log=stateSampler.keep(stateKey(s),now);
    if(!kt::safePacket(packet)||!packet.enabled||!same||prior.resetSerial!=packet.resetSerial||prior.dryRun!=packet.dryRun) {
        ++cancellationGeneration;forceBaseline=true;
    }
    latest=s;ReleaseSRWLockExclusive(&snapshotLock);++published;
    if(log) {Audit a;a.kind=1;a.thread=GetCurrentThreadId();a.ticks=t;a.snapshot=s;enqueue(a);}
    return 0;
}
int __fastcall publish(LState* L) {
    ++inFlight;
    __try {return publishImpl(L);}
    __finally {--inFlight;}
}

bool updateLua(void* system,bool reportFault=false) {
    auto L=readState(system);int height=0;
    if(!L||!stackHeight(L,height)||!lua.checkstack(L,8)) return false;
    if(L!=registeredState) {registeredState=L;callbackRegistered=false;invalidate();}
    if(!reportFault&&!callbackRegistered) {
        lua.getfield(L,-10002,"_KTNativePublish");
        const bool vacant=lua.type(L,-1)==0;
        if(!restoreStack(L,height)||!vacant) {errorAudit("publisher_name_collision");fatal=true;return false;}
        lua.pushcclosure(L,publish,0);lua.setfield(L,-10002,"_KTNativePublish");
        callbackRegistered=true;errorAudit("publisher_registered");
    }
    lua.getfield(L,-10002,"KuttenbergThief");
    if(lua.type(L,-1)!=5) {restoreStack(L,height);return false;}
    lua.getfield(L,-1,"nativeBridge");
    if(lua.type(L,-1)!=5) {restoreStack(L,height);return false;}
    lua.getfield(L,-1,reportFault?"NativeFault":"Tick");
    if(lua.type(L,-1)!=6) {restoreStack(L,height);return false;}
    int status=lua.pcall(L,0,0,0);
    bool restored=restoreStack(L,height);
    if(status) {++publishErrors;errorAudit("lua_tick_error");}
    return status==0&&restored;
}
bool protectedUpdate(void* system,bool reportFault=false) {
    __try {return updateLua(system,reportFault);}
    __except(EXCEPTION_EXECUTE_HANDLER) {luaUnsafe=true;fatal=true;++publishErrors;return false;}
}
std::uintptr_t __fastcall scriptUpdate(void* system) {
    ++inFlight;
    __try {
      auto result=originalUpdate(system);
      if(running.load()) {
        DWORD current=GetCurrentThreadId(),expected=0;
        scriptThread.compare_exchange_strong(expected,current);
        if(scriptThread.load()!=current) {luaUnsafe=true;if(!fatal.exchange(true)) {invalidate();errorAudit("script_thread_changed");}}
        // Only a logger fault with intact Lua ownership may report through Lua.
        // An API/stack SEH or ownership failure must never re-enter that VM.
        else if(fatal.load()) {
            if(!luaUnsafe.load()&&auditFault.load()!=AuditFault::None&&!faultReported)
                faultReported=protectedUpdate(system,true);
        }
        else if(!protectedUpdate(system)) invalidate();
      }
      return result;
    } __finally {--inFlight;}
}

bool readRequest(void* request,std::uint64_t& flags,kt::Vec3& v) {
    __try {
        auto p=static_cast<unsigned char*>(request);float values[3];
        memcpy(&flags,p,8);memcpy(values,p+0x7c,12);v={values[0],values[1],values[2]};return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool forwardXY(void* controller,void* request,float x,float y,float oldX,float oldY,
               unsigned long long expectedCancellation,bool& changed) {
    bool result=false;auto p=static_cast<unsigned char*>(request);
    changed=false;
    __try {
        if(running.load()&&!fatal.load()&&!forceBaseline.load()&&cancellationGeneration.load()==expectedCancellation) {
            memcpy(p+0x7c,&x,4);memcpy(p+0x80,&y,4);changed=true;
        }
        result=originalRequest(controller,request);
    } __finally {
        // Restore only this call's original stack request, including unwind.
        // KCD2 has already merged the bounded request into its own controller.
        if(changed) {memcpy(p+0x7c,&oldX,4);memcpy(p+0x80,&oldY,4);}
    }
    return result;
}
bool sameCommitSnapshot(const kt::NativeSnapshot& expected,double now) {
    const auto& p=latest.packet;const auto& old=expected.packet;
    return latest.valid&&kt::safePacket(p)&&p.enabled&&p.sequence==old.sequence&&p.epoch==old.epoch&&
        p.generation==old.generation&&p.identity==old.identity&&p.entity==old.entity&&p.resetSerial==old.resetSerial&&
        p.requestSerial==old.requestSerial&&p.dryRun==old.dryRun&&now>=latest.wallstamp&&now-latest.wallstamp<=0.15&&
        now>=latest.observerWallstamp&&now-latest.observerWallstamp<=0.25;
}
bool finishRequest(void* controller,void* request,Audit& a,bool locked,bool reserved,
                   Journal::Ticket ticket,unsigned long long decisionGeneration) {
    bool result=false;
    __try {
        if(KT_ALLOW_MOTION&&reserved&&a.wouldApply&&!a.snapshot.packet.dryRun&&running.load()&&!fatal.load()&&
           cancellationGeneration.load()==decisionGeneration) {
            result=forwardXY(controller,request,static_cast<float>(a.decision.motion.request.x),static_cast<float>(a.decision.motion.request.y),
                             static_cast<float>(a.original.x),static_cast<float>(a.original.y),decisionGeneration,a.wrote);
        } else result=originalRequest(controller,request);
    } __finally {
        if(a.wrote) ++actualModify;
        if(locked) ReleaseSRWLockExclusive(&coreLock);
        // An exceptional host return must not strand a reserved queue slot or
        // make shutdown wait forever. forwardXY has already restored its input.
        if(reserved) auditQueue.commit(ticket,a);
        --inFlight;
    }
    return result;
}
bool __fastcall movement(void* controller,void* request) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls;
    if(caller!=gameBase+ktretail::PlayerReturn||!running.load()) {
        bool result=originalRequest(controller,request);--inFlight;return result;
    }
    ++playerCalls;
    if(fatal.load()) {bool result=originalRequest(controller,request);--inFlight;return result;}
    Audit a;a.kind=0;a.thread=GetCurrentThreadId();a.ticks=ticks();
    std::uint64_t actor=0;
    bool owner=ktwin::owner(gameBase,controller,a.owner,actor);
    bool read=readRequest(request,a.flags,a.original);
    bool snapshot=copySnapshot(a.snapshot);
    a.decision.motion.request=a.original;
    bool locked=owner&&read&&snapshot&&!fatal.load()&&TryAcquireSRWLockExclusive(&coreLock);
    const auto decisionGeneration=cancellationGeneration.load();
    if(locked) {
        if(forceBaseline.exchange(false)||appliedBaseline!=decisionGeneration) {
            adapter.invalidate();appliedBaseline=decisionGeneration;
        }
        kt::AdapterInput input;input.snapshot=a.snapshot;input.now=seconds(a.ticks);
        input.ownerEntity=a.owner;input.actorToken=actor;input.controllerToken=reinterpret_cast<std::uintptr_t>(controller);
        input.flags=a.flags;input.request=a.original;a.decision=adapter.step(input);
    } else {++requestContentions;++cancellationGeneration;forceBaseline=true;a.decision.reason=kt::AdapterReason::Contention;}
    if(a.decision.motion.modified) ++wouldModify;
    // Recheck immediately before application. Keep the serialized action owner
    // until the current host request has finished; no newer worker can clear a
    // cancellation and resurrect an older decision.
    bool lease=a.decision.motion.modified&&TryAcquireSRWLockShared(&snapshotLock);
    if(lease) {
        a.wouldApply=running.load()&&!fatal.load()&&!forceBaseline.load()&&
            cancellationGeneration.load()==decisionGeneration&&sameCommitSnapshot(a.snapshot,seconds(ticks()));
        ReleaseSRWLockShared(&snapshotLock);
    }
    // Reserve audit space BEFORE touching host input. The writer never shares a
    // lock with the physics callback. A busy reservation cancels this action;
    // full queue, bounded session or disk failure latch all movement off.
    const bool log=locked&&requestSampler.keep(requestKey(a),seconds(a.ticks),a.decision.motion.modified);
    Journal::Ticket ticket;const bool reserved=log&&reserveAudit(ticket);
    if(a.decision.motion.modified&&!reserved) {
        ++cancellationGeneration;forceBaseline=true;adapter.stop();a.wouldApply=false;
    }
    return finishRequest(controller,request,a,locked,reserved,ticket,decisionGeneration);
}

void writeLine(const char* line) {
    DWORD written=0;auto length=static_cast<DWORD>(strlen(line));
    if(!WriteFile(logFile,line,length,&written,nullptr)||written!=length) auditFailure(AuditFault::WriteFailure);
}
void vectorJson(kt::Vec3 v,char (&out)[128]) {
    if(!v.finite()) strcpy_s(out,"null");
    else sprintf_s(out,"[%.9g,%.9g,%.9g]",v.x,v.y,v.z);
}
DWORD WINAPI writer(void*) {
    Audit a;char line[1800];
    for(;;) {
        bool any=false;
        while(pop(a)) {
            any=true;
            if(a.kind==3) continue; // Over-budget reservation; no host write was allowed.
            const auto& s=a.snapshot;const auto& p=s.packet;
            if(a.kind==2) sprintf_s(line,"{\"event\":\"bridge_event\",\"qpc\":%lld,\"thread\":%lu,\"reason\":\"%s\"}\n",a.ticks,a.thread,a.error);
            else {
                char before[128],after[128];vectorJson(a.original,before);vectorJson(a.decision.motion.request,after);
                sprintf_s(line,"{\"event\":\"%s\",\"qpc\":%lld,\"thread\":%lu,\"epoch\":%llu,\"generation\":%llu,\"identity\":%llu,\"sequence\":%llu,\"entity\":%u,\"owner_entity\":%u,\"valid\":%s,\"known\":%u,\"safe\":%u,\"gate\":\"%s\",\"async_seconds\":%.9g,\"observer_sequence\":%llu,\"dt\":%.9g,\"observer_age\":%.9g,\"snapshot_age\":%.9g,\"request_serial\":%llu,\"reset_serial\":%llu,\"dry_run\":%s,\"enabled\":%s,\"flags\":%llu,\"eligible\":%s,\"identity_match\":%s,\"adapter_reason\":%u,\"motion_reason\":%u,\"gain\":%.9g,\"would_modify\":%s,\"would_apply\":%s,\"modified\":%s,\"before\":%s,\"candidate\":%s}\n",
                    a.kind==1?"state":"request",a.ticks,a.thread,p.epoch,p.generation,p.identity,p.sequence,p.entity,a.owner,
                    s.valid?"true":"false",p.knownMask,p.safeMask,p.reason,p.asyncSeconds,p.observerSequence,p.frameDt,p.observerAge,seconds(a.ticks)-s.wallstamp,
                    p.requestSerial,p.resetSerial,p.dryRun?"true":"false",p.enabled?"true":"false",a.flags,
                    a.decision.eligible?"true":"false",a.decision.identityMatch?"true":"false",static_cast<unsigned>(a.decision.reason),
                    static_cast<unsigned>(a.decision.motion.reason),a.decision.motion.gain,a.decision.motion.modified?"true":"false",a.wouldApply?"true":"false",a.wrote?"true":"false",before,after);
            }
            writeLine(line);
        }
        if(stopping.load()&&!any) break;Sleep(20);
    }
    sprintf_s(line,"{\"event\":\"stopped\",\"calls\":%llu,\"player_calls\":%llu,\"published\":%llu,\"publish_errors\":%llu,\"dropped\":%llu,\"would_modify\":%llu,\"modified\":%llu,\"fatal\":%s,\"audit_fault\":%u,\"audit_busy\":%llu,\"request_contentions\":%llu,\"audit_reserved\":%llu}\n",
              calls.load(),playerCalls.load(),published.load(),publishErrors.load(),dropped.load(),wouldModify.load(),actualModify.load(),fatal.load()?"true":"false",
              static_cast<unsigned>(auditFault.load()),auditBusy.load(),requestContentions.load(),auditReserved.load());
    writeLine(line);FlushFileBuffers(logFile);return 0;
}
}

extern "C" __declspec(dllexport) DWORD WINAPI KtSwoopStart(void*) {
    if(attempted.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    DWORD verified=ktwin::verifyTarget(gameBase);if(verified) return verified;
    lua.bind(gameBase);LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);qpcFrequency=static_cast<double>(frequency.QuadPart);
    HMODULE pin;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&KtSwoopStart),&pin)) return GetLastError();
    wchar_t folder[32768],path[32768];if(!GetModuleFileNameW(ownModule,folder,32768)) return GetLastError();
    auto last=wcsrchr(folder,L'\\');if(!last) return ERROR_BAD_PATHNAME;*last=0;
    swprintf_s(path,L"%s\\state-bridge-%lu-%llu.jsonl",folder,GetCurrentProcessId(),GetTickCount64());
    logFile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logFile==INVALID_HANDLE_VALUE) return GetLastError();
    const kt::DiagnosticSwoopSettings tuning;
    char header[640];sprintf_s(header,"{\"event\":\"started\",\"protocol\":2,\"audit_revision\":2,\"max_records\":%u,\"idle_sample_seconds\":1,\"motion_compiled\":%s,\"default_dry_run\":true,\"qpc_frequency\":%lld,\"snapshot_ttl\":0.15,\"observer_ttl\":0.25,\"duration_limit\":%.3f,\"peak_gain\":%.3f,\"request_travel_budget\":%.3f,\"horizontal_cap\":%.3f}\n",MaxRecords,KT_ALLOW_MOTION?"true":"false",frequency.QuadPart,tuning.duration,tuning.peakGain,tuning.travelBudget,kt::MaxPrototypeHorizontalRequest);writeLine(header);
    requestTarget=reinterpret_cast<void*>(gameBase+ktretail::Request);updateTarget=reinterpret_cast<void*>(gameBase+ktretail::ScriptUpdate);
    bool initialized=MH_Initialize()==MH_OK;
    bool requestCreated=initialized&&MH_CreateHook(requestTarget,&movement,reinterpret_cast<void**>(&originalRequest))==MH_OK;
    bool updateCreated=requestCreated&&MH_CreateHook(updateTarget,&scriptUpdate,reinterpret_cast<void**>(&originalUpdate))==MH_OK;
    if(!updateCreated) {
        if(requestCreated) MH_RemoveHook(requestTarget);if(initialized) MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;
    }
    running=true;stopping=false;writerHandle=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
    bool enabled=writerHandle&&MH_QueueEnableHook(requestTarget)==MH_OK&&MH_QueueEnableHook(updateTarget)==MH_OK&&MH_ApplyQueued()==MH_OK;
    if(!enabled) {
        running=false;MH_DisableHook(requestTarget);MH_DisableHook(updateTarget);stopping=true;
        if(writerHandle) {WaitForSingleObject(writerHandle,INFINITE);CloseHandle(writerHandle);writerHandle=nullptr;}
        // Keep allocated hook metadata/trampolines pinned on uncertain startup.
        CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;
    }
    return ERROR_SUCCESS;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtSwoopStop(void*) {
    if(stopBusy.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    if(!writerHandle) {stopBusy=false;return ERROR_NOT_READY;}
    running=false;forceBaseline=true;++cancellationGeneration;
    auto r=MH_DisableHook(requestTarget),u=MH_DisableHook(updateTarget);
    if((r!=MH_OK&&r!=MH_ERROR_DISABLED)||(u!=MH_OK&&u!=MH_ERROR_DISABLED)) {stopBusy=false;return ERROR_INVALID_FUNCTION;}
    for(unsigned i=0;inFlight.load()&&i<2000;++i) Sleep(1);
    if(inFlight.load()) {stopBusy=false;return ERROR_BUSY;}
    stopping=true;WaitForSingleObject(writerHandle,INFINITE);CloseHandle(writerHandle);writerHandle=nullptr;
    CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;stopBusy=false;
    // Lua C closure and native trampolines intentionally remain pinned, inert.
    return ERROR_SUCCESS;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {ownModule=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}
