// The normal build observes existing attempts. The dry guard build suppresses
// the fixture handle after the native spatial query.
#include "../StateBridge/WindowsTarget.h"
#include "../StateBridge/AuditQueue.h"
#include "Selection.h"
#ifdef KT_LEDGE_DRY_GUARD
#include "../GuideProbe/Guide.h"
#include "DryGuide.h"
#endif
#include "MinHook.h"
#include <atomic>
#include <cmath>
#include <cwchar>
#include <intrin.h>
#ifdef KT_LEDGE_TALL_PREVIEW
#include "TallPreview.h"
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
#include "VaultTrial.h"
#endif
#ifdef KT_LEDGE_REPEATABLE
#include "RepeatableVault.h"
#endif

namespace {
ktledge::AttemptFn originalAttempt=nullptr;
ktledge::SelectionFn originalSelection=nullptr;
ktledge::QueryFn originalQuery=nullptr;
ktledge::FrameworkFn originalFramework=nullptr;
#ifdef KT_LEDGE_DRY_GUARD
ktguide::IndexFn originalIndex=nullptr;
std::atomic<bool> activationArmed{false};
std::atomic<unsigned> dryActivations{0},guardedQueries{0};
std::atomic<unsigned> dryIndexResults[static_cast<unsigned>(ktledge::DryIndexResult::Count)]{};
#ifdef KT_LEDGE_DRY_SELECTION
std::atomic<unsigned> withheldSelections{0};
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
#ifdef KT_LEDGE_REPEATABLE
std::atomic<std::uint64_t> indexEpoch{0},activeEpoch{0},nextTrial{0};
SRWLOCK traversalState=SRWLOCK_INIT;
ktvault::JumpEdge jumpEdge;
bool freshJump(std::uint64_t epoch,std::uint32_t entity,const void* input);
#else
std::atomic<bool> trialArmedOnce{false};
std::atomic<unsigned long long> trialDeadline{0};
#endif
std::atomic<unsigned> trialResult{0},trialFrameworkForwards{0},trialGroundSamples{0};
std::atomic<bool> trialUsedTakeoff{false};
struct GroundFrame {
    ktcapsule::Vector position;std::uint32_t entity=0;std::uint64_t tick=0;
#ifdef KT_LEDGE_REPEATABLE
    std::uint64_t epoch=0;
    std::uint32_t handle=0;
    std::uintptr_t actor=0;
#endif
};
#ifdef KT_LEDGE_REPEATABLE
GroundFrame lastGround;
#else
thread_local GroundFrame lastGround;
#endif
#ifdef KT_LEDGE_FIXTURE
bool fixtureClearance=false;
#endif
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
using ParamsFn=void* (__fastcall *)(void*,std::uint32_t);
ParamsFn originalParams=nullptr;
std::atomic<unsigned> tallClones{0};
constexpr unsigned HookCount=6;
#else
constexpr unsigned HookCount=5;
#endif
#else
constexpr unsigned HookCount=4;
#endif
std::uintptr_t gameBase=0;
HMODULE ownModule=nullptr;
void* targets[HookCount]{};
HANDLE writerThread=nullptr,logFile=INVALID_HANDLE_VALUE;
std::atomic<bool> attempted{false},recording{false},stopping{false},stopBusy{false},writeFailed{false},truncated{false};
std::atomic<unsigned> inFlight{0};
std::atomic<unsigned long long> calls[4]{},started{0},ended{0},ownerRefused{0},unreadable{0},contextOverflow{0},
    unlinkedFramework{0},dropped{0},reserved{0},committed{0};
constexpr unsigned Capacity=4096,MaxRecords=60000,MaxDepth=4;
// Closed diagnostic vocabulary. No pointer value is queued or logged.
enum class RequestGuard : unsigned {
    NotObserved,Matched,NullHolder,UnreadableHolder,EmptyHolder,UnreadableVtable,VtableMismatch,
    CanonicalOwnerInvalid,CanonicalEntityMismatch,UnreadableComponent,NullComponent,UnreadableComponentVtable,
    ComponentVtableMismatch,UnreadableComponentPlayer,ComponentPlayerMismatch,
    UnreadableActionOwner,NullActionOwner,ActionOwnerMismatch,
    NullFramework,UnreadableCore,NullCore,UnreadableResource,NullResource,UnreadableFrameworkActor,
    FrameworkActorMismatch,UnreadableRequest,Count
};
constexpr const char* GuardNames[]={
    "not_observed","matched","null_holder","unreadable_holder","empty_holder","unreadable_action_vtable","action_vtable_mismatch",
    "canonical_owner_invalid","canonical_entity_mismatch","unreadable_action_component","null_action_component","unreadable_component_vtable",
    "component_vtable_mismatch","unreadable_component_player","component_player_mismatch",
    "unreadable_action_owner","null_action_owner","action_owner_mismatch",
    "null_framework","unreadable_framework_core","null_framework_core","unreadable_core_resource","null_core_resource","unreadable_framework_actor",
    "framework_actor_mismatch","unreadable_request"
};
static_assert(sizeof GuardNames/sizeof *GuardNames==static_cast<unsigned>(RequestGuard::Count),"guard vocabulary");
std::atomic<unsigned long long> requestGuards[static_cast<unsigned>(RequestGuard::Count)]{};
bool guardUnreadable(RequestGuard g) {
    return g==RequestGuard::UnreadableHolder||g==RequestGuard::UnreadableVtable||g==RequestGuard::UnreadableActionOwner||
        g==RequestGuard::UnreadableCore||g==RequestGuard::UnreadableResource||g==RequestGuard::UnreadableFrameworkActor||g==RequestGuard::UnreadableRequest||
        g==RequestGuard::UnreadableComponent||g==RequestGuard::UnreadableComponentVtable||g==RequestGuard::UnreadableComponentPlayer;
}
struct Sample {
    std::uint64_t transaction=0;
    long long begin=0,end=0,queryTick=0,selectionTick=0,frameworkTick=0;
    DWORD thread=0;
    std::uint32_t entity=0,queryHandle=0xffffffffu;
    unsigned scope=0,depth=0,queries=0,selections=0,frameworks=0;
    float position[3]{},direction[3]{},bounds[4]{};
    ktledge::Selection selection;
    ktledge::Request request;
    RequestGuard requestGuard=RequestGuard::NotObserved;
    unsigned requestGuardBits=0;
    std::uint32_t actionVtableRva=0xffffffffu;
    std::uint32_t componentVtableRva=0xffffffffu;
    bool actionVtableRead=false,componentVtableRead=false;
    std::uint8_t attemptByte=0,frameworkByte=0;
    bool completed=false,exceptional=false,queryKnown=false,queryInputsKnown=false,queryReturnMatches=false,
        selectionKnown=false,requestKnown=false,frameworkReturned=false,attemptReturned=false,allowAlternate=false,contextSuppressed=false;
#ifdef KT_LEDGE_REPEATABLE
    const char* climbGate=nullptr;
    bool retryLookup=false,recoveredLookup=false;
#endif
};
struct Frame {
    std::uintptr_t actor=0;
    Sample sample;
#ifdef KT_LEDGE_DRY_SELECTION
    bool previewActive=false;
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
    bool tallQuery=false,tallExtended=false;
    kttall::ParamBlock tallParams{};
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
    bool trialInput=false,trialReleased=false;
#endif
#ifdef KT_LEDGE_REPEATABLE
    std::uint64_t guideEpoch=0;
#endif
};
struct FrameStack {unsigned depth=0,suppressed=0;Frame frame[MaxDepth];};
thread_local FrameStack stack;
using Queue=kt::AuditQueue<Sample,Capacity>;
Queue queue;

long long ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
bool allPins(std::uintptr_t base) {
    __try {
        for(const auto& pin:ktledge::Hooks)
            if(memcmp(reinterpret_cast<void*>(base+pin.rva),pin.bytes,sizeof pin.bytes)) return false;
        for(const auto& pin:ktledge::Callers)
            if(memcmp(reinterpret_cast<void*>(base+pin.rva),pin.bytes,sizeof pin.bytes)) return false;
#ifdef KT_LEDGE_DRY_GUARD
        const auto& pin=ktledge::SecondQueryCaller;
        if(memcmp(reinterpret_cast<void*>(base+pin.rva),pin.bytes,sizeof pin.bytes)) return false;
        const auto& index=ktguide::Hooks[2];
        constexpr unsigned char indexCall[]={0xe8,0xa2,0x57,0x4b,0xff};
        if(memcmp(reinterpret_cast<void*>(base+index.rva),index.bytes,sizeof index.bytes)||
           memcmp(reinterpret_cast<void*>(base+ktguide::IndexReturn-5),indexCall,sizeof indexCall)) return false;
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
        if(!ktvault::pins(base)) return false;
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
        constexpr unsigned char call[]={0xe8,0xe2,0xd7,0x8e,0xff};
        if(!ktretail::bytesMatch(base,{kttall::ParamsRva,"8954241053574883ec38488bd98bfa48"})||
           memcmp(reinterpret_cast<void*>(base+kttall::SelectorReturn-5),call,sizeof call)) return false;
#endif
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool ownerId(void* actor,std::uint32_t& id) {
    __try {
        const auto a=reinterpret_cast<std::uintptr_t>(actor);
        if(!a) return false;
        std::uint64_t verified=0;
        return ktwin::owner(gameBase,*reinterpret_cast<void**>(a+0x180),id,verified)&&verified==a;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool readSelection(const ktledge::Selection* output,ktledge::Selection& value) {
    __try {memcpy(&value,output,sizeof value);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
// Fixture caller override is absent from the built DLL.
#ifdef KT_LEDGE_FIXTURE
thread_local bool fixtureCallers=false;
#endif
bool callerMatches(std::uintptr_t caller,std::uintptr_t expected) {
#ifdef KT_LEDGE_FIXTURE
    if(fixtureCallers) return true;
#endif
    return caller==gameBase+expected;
}
Frame* current() {return stack.depth&&!stack.suppressed?&stack.frame[stack.depth-1]:nullptr;}
bool suppressNested() {
    if(!stack.depth) return false;
    stack.frame[stack.depth-1].sample.contextSuppressed=true;++stack.suppressed;return true;
}
Frame* enter(void* actor,unsigned scope) {
#ifdef KT_LEDGE_REPEATABLE
    if(stack.suppressed||!activationArmed.load(std::memory_order_acquire)) return nullptr;
#else
    if(stack.suppressed||!recording.load(std::memory_order_acquire)) return nullptr;
#endif
    std::uint32_t entity=0;
    if(!ownerId(actor,entity)) {++ownerRefused;return nullptr;}
    if(stack.depth>=MaxDepth) {++contextOverflow;recording=false;return nullptr;}
    auto& f=stack.frame[stack.depth++];f=Frame{};
    f.actor=reinterpret_cast<std::uintptr_t>(actor);
    auto& s=f.sample;s.transaction=++started;s.begin=ticks();s.thread=GetCurrentThreadId();
    s.entity=entity;s.scope=scope;s.depth=stack.depth;
    return &f;
}
void enqueue(const Sample& sample) {
    auto count=reserved.load(std::memory_order_relaxed);bool budget=false;
    for(unsigned i=0;i<64;++i) {
        if(count>=MaxRecords) {truncated=true;recording=false;return;}
        if(reserved.compare_exchange_weak(count,count+1,std::memory_order_relaxed)) {budget=true;break;}
    }
    if(!budget) {++dropped;recording=false;return;}
    Queue::Ticket ticket;
    if(queue.reserve(ticket)!=Queue::Result::Reserved) {++dropped;recording=false;return;}
    queue.commit(ticket,sample);++committed;
    if(count+1==MaxRecords) {truncated=true;recording=false;}
}
void leave(Frame* f,bool normal) {
    if(!f) return;
    f->sample.completed=normal;f->sample.exceptional|=!normal;f->sample.end=ticks();
#ifdef KT_LEDGE_REPEATABLE
    if(recording) enqueue(f->sample);
#else
    enqueue(f->sample);
#endif
    ++ended;
    // Only scalar Sample enters the queue. Callback-scoped actor identity is
    // erased before returning, also on SEH/C++ unwind.
    *f=Frame{};--stack.depth;
}
bool queryInputs(void* manager,const float* position,const float* direction,const float* bounds,Sample& s) {
    __try {
        if(!manager||*reinterpret_cast<void**>(gameBase+ktledge::ManagerGlobalRva)!=manager) return false;
        memcpy(s.position,position,sizeof s.position);memcpy(s.direction,direction,sizeof s.direction);
        memcpy(s.bounds,bounds,sizeof s.bounds);
        for(float v:s.position) if(!std::isfinite(v)||std::abs(v)>1e7f) return false;
        for(float v:s.direction) if(!std::isfinite(v)||std::abs(v)>1e7f) return false;
        for(float v:s.bounds) if(!std::isfinite(v)||std::abs(v)>1e7f) return false;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool readHandle(const std::uint32_t* output,std::uint32_t& value) {
    __try {memcpy(&value,output,sizeof value);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
template<class T> bool guardedRead(const void* source,T& value) {
    __try {memcpy(&value,source,sizeof value);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
#ifdef KT_LEDGE_REPEATABLE
bool freshJump(std::uint64_t epoch,std::uint32_t entity,const void* input) {
    unsigned char bits[2]{};
    const auto known=guardedRead(input,bits);
    AcquireSRWLockExclusive(&traversalState);
    const auto fresh=jumpEdge.update(epoch,entity,known,bits);
    ReleaseSRWLockExclusive(&traversalState);
    return fresh;
}
#endif
RequestGuard frameworkRequest(void* framework,void* holder,Frame* f) {
    auto& s=f->sample;s.requestGuardBits=0;s.actionVtableRva=0xffffffffu;s.actionVtableRead=false;
    s.componentVtableRva=0xffffffffu;s.componentVtableRead=false;s.request=ktledge::Request{};
    if(!holder) return RequestGuard::NullHolder;
    std::uintptr_t action=0,vtable=0,actionOwner=0,component=0,player=0,core=0,resource=0,frameworkOwner=0;
    if(!guardedRead(holder,action)) return RequestGuard::UnreadableHolder;
    if(!action) return RequestGuard::EmptyHolder;
    s.requestGuardBits|=1u;
    if(!guardedRead(reinterpret_cast<void*>(action),vtable)) return RequestGuard::UnreadableVtable;
    s.actionVtableRead=true;
    if(vtable>=gameBase&&vtable-gameBase<ktretail::ImageSize) s.actionVtableRva=static_cast<std::uint32_t>(vtable-gameBase);
    if(vtable!=gameBase+ktledge::LedgeActionVtableRva) return RequestGuard::VtableMismatch;
    s.requestGuardBits|=2u;
    // +0x58 owns the separate WH C_ActionActor, not the canonical Cry C_Player.
    // Keep canonical identity strict, then prove the component's owning/back links.
    std::uint32_t entity=0;
    if(!ownerId(reinterpret_cast<void*>(f->actor),entity)) return RequestGuard::CanonicalOwnerInvalid;
    if(entity!=s.entity) return RequestGuard::CanonicalEntityMismatch;
    s.requestGuardBits|=4u;
    if(!guardedRead(reinterpret_cast<void*>(f->actor+ktledge::PlayerActionActorOffset),component)) return RequestGuard::UnreadableComponent;
    if(!component) return RequestGuard::NullComponent;
    s.requestGuardBits|=8u;
    if(!guardedRead(reinterpret_cast<void*>(component),vtable)) return RequestGuard::UnreadableComponentVtable;
    s.componentVtableRead=true;
    if(vtable>=gameBase&&vtable-gameBase<ktretail::ImageSize) s.componentVtableRva=static_cast<std::uint32_t>(vtable-gameBase);
    if(vtable!=gameBase+ktledge::ActionActorVtableRva) return RequestGuard::ComponentVtableMismatch;
    s.requestGuardBits|=16u;
    if(!guardedRead(reinterpret_cast<void*>(component+ktledge::ActionActorPlayerOffset),player)) return RequestGuard::UnreadableComponentPlayer;
    if(player!=f->actor) return RequestGuard::ComponentPlayerMismatch;
    s.requestGuardBits|=32u;
    if(!guardedRead(reinterpret_cast<void*>(action+0x58),actionOwner)) return RequestGuard::UnreadableActionOwner;
    if(!actionOwner) return RequestGuard::NullActionOwner;
    if(actionOwner!=component) return RequestGuard::ActionOwnerMismatch;
    s.requestGuardBits|=64u;
    if(!framework) return RequestGuard::NullFramework;
    if(!guardedRead(static_cast<unsigned char*>(framework)+8,core)) return RequestGuard::UnreadableCore;
    if(!core) return RequestGuard::NullCore;
    s.requestGuardBits|=128u;
    if(!guardedRead(reinterpret_cast<void*>(core+0x50),resource)) return RequestGuard::UnreadableResource;
    if(!resource) return RequestGuard::NullResource;
    s.requestGuardBits|=256u;
    if(!guardedRead(reinterpret_cast<void*>(resource+8),frameworkOwner)) return RequestGuard::UnreadableFrameworkActor;
    if(frameworkOwner!=component) return RequestGuard::FrameworkActorMismatch;
    s.requestGuardBits|=512u;
    if(!guardedRead(reinterpret_cast<void*>(action+0x68),s.request)) return RequestGuard::UnreadableRequest;
    s.requestGuardBits|=1024u;
    return RequestGuard::Matched;
}
const char* vtableKind(const Sample& s) {
    if(!s.actionVtableRead) return "unobserved";
    if(s.actionVtableRva==0xffffffffu) return "outside_target";
    if(s.actionVtableRva==ktledge::LedgeActionVtableRva) return "ledge_primary";
    if(s.actionVtableRva==ktledge::LedgeActionSecondary10VtableRva) return "ledge_secondary_10";
    if(s.actionVtableRva==ktledge::LedgeActionSecondary50VtableRva) return "ledge_secondary_50";
    return "other_target_rva";
}

const char* componentKind(const Sample& s) {
    if(!s.componentVtableRead) return "unobserved";
    if(s.componentVtableRva==0xffffffffu) return "outside_target";
    return s.componentVtableRva==ktledge::ActionActorVtableRva?"action_actor":"other_target_rva";
}

std::uint8_t __fastcall observedAttempt(std::uintptr_t opaque1,void* actor,std::uintptr_t opaque3,
                                      std::uintptr_t restriction,void* input) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls[0];Frame* f=nullptr;std::uint8_t result=0;bool normal=false,muted=false;
    __try {
        if(callerMatches(caller,ktledge::AttemptReturnRva)) f=enter(actor,0);
#ifdef KT_LEDGE_LIVE_TRIAL
#ifdef KT_LEDGE_REPEATABLE
        if(f) {
            f->guideEpoch=activeEpoch.load();
            f->trialInput=freshJump(f->guideEpoch,f->sample.entity,input);
            if(f->trialInput) f->sample.climbGate="no_owned_selection";
        }
#else
        unsigned char bits[2]{};
        if(f&&guardedRead(input,bits)) f->trialInput=bits[0]==1&&bits[1]==0;
#endif
#endif
        if(!f) muted=suppressNested();
        result=originalAttempt(opaque1,actor,opaque3,restriction,input);normal=true;
    } __finally {
        if(f) {f->sample.attemptByte=result;f->sample.attemptReturned=normal;leave(f,normal);}
        if(muted) --stack.suppressed;
        --inFlight;
    }
    return result;
}
#ifdef KT_LEDGE_LIVE_TRIAL
#ifdef KT_LEDGE_REPEATABLE
bool activeGuide(std::uint64_t epoch) {
    if(!activationArmed||!epoch||epoch!=activeEpoch.load()||epoch!=indexEpoch.load()) return false;
#ifdef KT_LEDGE_FIXTURE
    if(fixtureCallers) return true;
#endif
    __try {
        auto* manager=*reinterpret_cast<const unsigned char**>(gameBase+ktledge::ManagerGlobalRva);
        if(!manager||manager[0x60028]||
           *reinterpret_cast<const unsigned*>(manager+0x10)!=ktledge::DryLedgeCount||
           *reinterpret_cast<const unsigned*>(manager+0x14)!=ktledge::DryMarkerCount) return false;
        const auto* ledges=*reinterpret_cast<const unsigned char* const*>(manager);
        const auto* markers=*reinterpret_cast<const unsigned char* const*>(manager+8);
        if(!ledges||!markers) return false;
        for(unsigned index=0;index<ktledge::DryGuideCount;++index) {
            unsigned char expected[ktledge::LedgeSize];
            ktledge::dryTail(index,true,expected);
            if(memcmp(ledges+(ktledge::DryGuideObject+index)*ktledge::LedgeSize,expected,sizeof expected)) return false;
        }
        std::uint64_t hash=0xcbf29ce484222325ull;
        for(unsigned i=ktledge::DryMarkerStart;i<ktledge::DryMarkerCount;++i)
            for(unsigned j=0;j<25;++j) ktledge::hashByte(hash,markers[i*28+j]);
        return hash==ktledge::ActiveMarkerHash&&epoch==activeEpoch.load()&&epoch==indexEpoch.load();
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
#endif
bool trialCandidate(const Frame& frame,const ktledge::Selection& selection) {
    const auto& s=frame.sample;
#ifdef KT_LEDGE_REPEATABLE
    const auto* wall=ktvault::wallFor(selection.handle);
    return wall&&s.scope==0&&selection.actionType==4&&
        (selection.handle!=ktvault::Handle||frame.tallExtended)&&activeGuide(frame.guideEpoch)&&
        s.queryKnown&&s.queryReturnMatches&&s.queryHandle==selection.handle&&
        s.queries==1&&s.selections==1&&ktvault::aim(*wall,s.direction);
#else
#ifdef KT_LEDGE_TALL_PREVIEW
    if(!frame.tallExtended) return false;
#endif
    return s.scope==0&&selection.actionType==4&&selection.handle==ktvault::Handle&&
        recording&&
        s.queryKnown&&s.queryHandle==selection.handle&&s.queries==1&&s.selections==1&&
        s.direction[0]*ktvault::Facing.x+s.direction[1]*ktvault::Facing.y>=0.98f&&
        std::abs(s.direction[2])<=0.01f;
#endif
}
void rememberGround(Frame& frame,void* actor,const ktledge::Selection& selection) {
#ifndef KT_LEDGE_REPEATABLE
    lastGround={};
#endif
    if(!trialCandidate(frame,selection)) return;
#ifdef KT_LEDGE_FIXTURE
    if(fixtureClearance) return;
#endif
    ktvault::Contact contact;
    if(ktvault::inspect(gameBase,actor,contact)==ktvault::Result::Released&&contact.grounded) {
#ifdef KT_LEDGE_REPEATABLE
        ktcapsule::Corridor route;
        if(!ktvault::approachRoute(*ktvault::wallFor(selection.handle),contact.position,frame.sample.direction,
                                  route,frame.sample.recoveredLookup)) return;
        const auto tick=GetTickCount64();
        AcquireSRWLockExclusive(&traversalState);
        if(tick>=lastGround.tick)
            lastGround={contact.position,frame.sample.entity,tick,frame.guideEpoch,selection.handle,frame.actor};
        ReleaseSRWLockExclusive(&traversalState);
#else
        lastGround={contact.position,frame.sample.entity,GetTickCount64()};
#endif
        ++trialGroundSamples;
    }
}
ktvault::Result trialClearance(Frame& frame,void* actor) {
#ifdef KT_LEDGE_FIXTURE
    if(fixtureClearance) return ktvault::Result::Released;
#endif
    ktvault::Contact contact;
    const auto result=ktvault::inspect(gameBase,actor,contact);
    if(result!=ktvault::Result::Released) return result;
    if(!contact.grounded) {
#ifdef KT_LEDGE_REPEATABLE
        AcquireSRWLockShared(&traversalState);
        const auto ground=lastGround;
        ReleaseSRWLockShared(&traversalState);
#else
        const auto& ground=lastGround;
#endif
        const auto now=GetTickCount64();
        if(ground.entity!=frame.sample.entity||!ground.tick||now<ground.tick||
#ifdef KT_LEDGE_REPEATABLE
           ground.epoch!=frame.guideEpoch||ground.handle!=frame.sample.selection.handle||ground.actor!=frame.actor||
#endif
           !ktvault::immediateTakeoff(contact,ground.position,now-ground.tick)) return ktvault::Result::Ground;
        trialUsedTakeoff=true;
    }
#ifdef KT_LEDGE_REPEATABLE
    ktcapsule::Corridor route;
    if(!ktvault::approachRoute(*ktvault::wallFor(frame.sample.selection.handle),contact.position,frame.sample.direction,
                              route,frame.sample.recoveredLookup))
        return ktvault::Result::Position;
    return ktvault::checkClearance(gameBase,actor,route);
#else
    return ktvault::checkClearance(gameBase,actor);
#endif
}
bool releaseTrial(Frame& frame,void* actor,const ktledge::Selection& selection) {
    if(!trialCandidate(frame,selection)||frame.trialReleased||!frame.trialInput) return false;
#ifdef KT_LEDGE_REPEATABLE
    if(!ktvault::claimCooldown(nextTrial,GetTickCount64())) {
        frame.sample.climbGate="cooldown";
        return false;
    }
#else
    auto expiry=trialDeadline.load();
    if(!expiry||GetTickCount64()>expiry||!trialDeadline.compare_exchange_strong(expiry,0)) return false;
#endif
    const auto result=trialClearance(frame,actor);
    trialResult=static_cast<unsigned>(result);
#ifdef KT_LEDGE_REPEATABLE
    constexpr const char* GateNames[]={"not_run","physical","ground","stance","approach","clearance","released"};
    frame.sample.climbGate=GateNames[static_cast<unsigned>(result)];
#endif
    frame.trialReleased=result==ktvault::Result::Released
#ifdef KT_LEDGE_REPEATABLE
        &&activeGuide(frame.guideEpoch)
#endif
        ;
    return frame.trialReleased;
}
#endif
void __fastcall observedSelection(void* actor,ktledge::Selection* output,bool alternate) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls[1];Frame* f=nullptr;bool own=false,normal=false,muted=false;
    __try {
        if(callerMatches(caller,ktledge::ReturnRva)) {
            f=current();
            if(!f||f->actor!=reinterpret_cast<std::uintptr_t>(actor)) {f=enter(actor,1);own=f!=nullptr;}
            if(f) {++f->sample.selections;f->sample.allowAlternate=alternate;}
        }
        if(!f) muted=suppressNested();
#ifdef KT_LEDGE_DRY_SELECTION
        if(f) f->previewActive=true;
#endif
        originalSelection(actor,output,alternate);normal=true;
        if(f) {
            std::uint32_t id=0;
            f->sample.selectionKnown=ownerId(actor,id)&&id==f->sample.entity&&readSelection(output,f->sample.selection);
            if(!f->sample.selectionKnown) ++unreadable;
            f->sample.selectionTick=ticks();
        }
#ifdef KT_LEDGE_DRY_SELECTION
        // Only the pinned, scoped selector may see the fixture. Its caller
        // receives the ordinary refusal pair before creating an action.
#ifdef KT_LEDGE_LIVE_TRIAL
        if(f&&!f->trialInput) rememberGround(*f,actor,f->sample.selection);
#endif
        if(f&&ktledge::dryQueryResult(output->handle)!=output->handle) {
#ifdef KT_LEDGE_LIVE_TRIAL
            if(!f->sample.selectionKnown||!releaseTrial(*f,actor,*output))
#endif
            {
            *output=ktledge::Selection{};
            ++withheldSelections;
            }
        }
#endif
    } __finally {
#ifdef KT_LEDGE_DRY_SELECTION
        if(f) f->previewActive=false;
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
        if(f) {f->tallQuery=false;f->tallExtended=false;}
#endif
        if(f) {f->sample.exceptional|=!normal;if(own) leave(f,normal);}
        if(muted) --stack.suppressed;
        --inFlight;
    }
}
std::uint32_t* __fastcall observedQuery(void* manager,std::uint32_t* output,const float* position,const float* direction,
                                      float a,float b,float c,float d) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls[2];Frame* f=nullptr;std::uint32_t* result=nullptr;bool normal=false,muted=false;
    __try {
        if(callerMatches(caller,ktledge::QueryReturnRva)) {
            f=current();
            if(f) {++f->sample.queries;const float bounds[]={a,b,c,d};
                f->sample.queryInputsKnown=queryInputs(manager,position,direction,bounds,f->sample);}
        }
        if(!f) muted=suppressNested();
        result=originalQuery(manager,output,position,direction,a,b,c,d);normal=true;
        if(f) {
            f->sample.queryKnown=f->sample.queryInputsKnown&&readHandle(output,f->sample.queryHandle);
            f->sample.queryReturnMatches=result==output;f->sample.queryTick=ticks();
            if(!f->sample.queryKnown) ++unreadable;
#ifdef KT_LEDGE_REPEATABLE
            auto& sample=f->sample;
            if(f->previewActive&&sample.scope==0&&sample.queries==1&&sample.queryKnown&&
               sample.queryReturnMatches&&sample.queryHandle==ktledge::NoGuide&&
               activeGuide(f->guideEpoch)&&ktvault::tallRetryInRange(sample.position,sample.direction)) {
                sample.retryLookup=true;
                std::uint32_t retry=ktledge::NoGuide;
                const auto returned=originalQuery(manager,&retry,ktvault::TallLookupAnchor,ktvault::TallLookupDirection,a,b,c,d);
                if(returned==&retry&&retry==ktvault::Handle&&activeGuide(f->guideEpoch)) {
                    *output=retry;
                    sample.queryHandle=retry;
                    sample.recoveredLookup=true;
                }
                sample.queryTick=ticks();
            }
#endif
        }
#ifdef KT_LEDGE_DRY_GUARD
        // The native wrapper returns its caller-owned output synchronously.
        // Keep the raw result in the audit, then suppress the fixture before
        // either caller sees it. The guard stays active if recording stops.
        const auto raw=*output;
        const auto permitted=ktledge::dryQueryResult(raw);
        if(permitted!=raw) {
#ifdef KT_LEDGE_DRY_SELECTION
            if(!f||!f->previewActive||!f->sample.queryInputsKnown||!dryActivations.load()
#ifdef KT_LEDGE_REPEATABLE
               ||!activeGuide(f->guideEpoch)
#endif
               )
#endif
            {*output=permitted;++guardedQueries;}
#ifdef KT_LEDGE_TALL_PREVIEW
            else f->tallQuery=(raw>>16)==ktledge::DryGuideObject;
#endif
        }
#endif
    } __finally {if(f) f->sample.exceptional|=!normal;if(muted) --stack.suppressed;--inFlight;}
    return result;
}
#ifdef KT_LEDGE_TALL_PREVIEW
void* __fastcall observedParams(void* bank,std::uint32_t profile) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;
    void* result=nullptr;
    __try {
        result=originalParams(bank,profile);
        auto* frame=current();
        if(result&&callerMatches(caller,kttall::SelectorReturn)&&frame&&frame->previewActive&&
           frame->tallQuery&&guardedRead(result,frame->tallParams)&&kttall::extendHeight(frame->tallParams)) {
            result=frame->tallParams;
            frame->tallExtended=true;
            ++tallClones;
        }
    } __finally {--inFlight;}
    return result;
}
#endif
#ifdef KT_LEDGE_DRY_GUARD
ktledge::DryIndexResult enableOwnedGuide(void* manager,bool allowEnabled=false) {
    using Result=ktledge::DryIndexResult;
    __try {
        if(!manager||manager!=*reinterpret_cast<void**>(gameBase+ktledge::ManagerGlobalRva)) return Result::Manager;
        auto* bytes=static_cast<unsigned char*>(manager);
        if(bytes[0x60028]) return Result::Editor;
        auto* ledges=*reinterpret_cast<unsigned char**>(bytes);
        auto* markers=*reinterpret_cast<const unsigned char**>(bytes+8);
        const auto result=ktledge::checkDryMember(ledges,*reinterpret_cast<unsigned*>(bytes+0x10),
                                                  markers,*reinterpret_cast<unsigned*>(bytes+0x14),allowEnabled);
        if(result!=Result::Matched) return result;
        // The installed member stays inactive. Only the original index builder
        // sees this verified fixture bit after all of the dry guard hooks exist.
        for(unsigned index=0;index<ktledge::DryGuideCount;++index)
            memcpy(ledges+(ktledge::DryGuideObject+index)*ktledge::LedgeSize+8,
                   &ktledge::DrySideFlags,sizeof ktledge::DrySideFlags);
        return Result::Matched;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return Result::Fault;}
}

void __fastcall guardedIndex(void* manager) {
    const auto address=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;
    __try {
        using Result=ktledge::DryIndexResult;
#ifdef KT_LEDGE_REPEATABLE
        const auto epoch=++indexEpoch;
        activeEpoch=0;
#endif
        const auto result=!activationArmed.load()?Result::ArmedOff:
#ifdef KT_LEDGE_REPEATABLE
            address!=gameBase+ktguide::IndexReturn?Result::Caller:enableOwnedGuide(manager,true);
#else
            address!=gameBase+ktguide::IndexReturn?Result::Caller:enableOwnedGuide(manager);
#endif
        ++dryIndexResults[static_cast<unsigned>(result)];
        originalIndex(manager);
        if(result==Result::Matched) {
            ++dryActivations;
#ifdef KT_LEDGE_REPEATABLE
            if(activationArmed&&epoch==indexEpoch.load()) activeEpoch=epoch;
#endif
        }
    } __finally {--inFlight;}
}
#endif
std::uint8_t __fastcall observedFramework(void* framework,void* holder) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls[3];Frame* f=nullptr;std::uint8_t result=0;bool normal=false,muted=false;
    __try {
        if(callerMatches(caller,ktledge::FrameworkReturnRva)) {
            f=current();
            if(f) {++f->sample.frameworks;f->sample.requestGuard=frameworkRequest(framework,holder,f);
                ++requestGuards[static_cast<unsigned>(f->sample.requestGuard)];
                f->sample.requestKnown=f->sample.requestGuard==RequestGuard::Matched;
                if(guardUnreadable(f->sample.requestGuard)) ++unreadable;}
            else ++unlinkedFramework;
        }
        if(!f) muted=suppressNested();
        // The smart pointer may release/destroy the action here. Its request was
        // copied before this call; never read the action again after return.
#ifdef KT_LEDGE_LIVE_TRIAL
        if(f&&f->trialReleased) {
            if(f->sample.requestKnown&&f->sample.request.selection.actionType==4&&
#ifdef KT_LEDGE_REPEATABLE
               f->sample.request.selection.handle==f->sample.selection.handle&&
               f->sample.frameworks==1&&activeGuide(f->guideEpoch)
#else
               f->sample.request.selection.handle==ktvault::Handle&&f->sample.frameworks==1
#endif
               ) {
                ++trialFrameworkForwards;
                result=originalFramework(framework,holder);
            }
        } else
#endif
        {result=originalFramework(framework,holder);}
        normal=true;
        if(f) {f->sample.frameworkByte=result;f->sample.frameworkReturned=true;f->sample.frameworkTick=ticks();}
    } __finally {if(f) f->sample.exceptional|=!normal;if(muted) --stack.suppressed;--inFlight;}
    return result;
}
bool requestMatches(const Sample& s) {
    return s.requestKnown&&s.selectionKnown&&s.request.selection.actionType==s.selection.actionType&&
        s.request.selection.handle==s.selection.handle;
}
const char* stage(const Sample& s) {
    if(!s.completed||s.exceptional) return "host_exception";
    if(s.contextSuppressed) return "context_unobserved";
    if(s.scope==0&&(!s.attemptReturned||s.attemptByte>1)) return "attempt_result_unknown";
    if(s.queries>1||s.selections>1||s.frameworks>1) return "ambiguous_multiple_calls";
    if(!s.queries) return "before_query";
    if(!s.queryKnown||!s.queryReturnMatches) return "query_unknown";
    if(s.queryHandle==0xffffffffu) return "no_query_handle";
    if(!s.selectionKnown) return "selection_unknown";
    if(s.selection.actionType==-1||s.selection.handle==0xffffffffu) return "selection_refused";
    if(s.selection.handle!=s.queryHandle) return "query_selection_mismatch";
    if(s.scope==1) return "selected_only";
    if(!s.frameworks) return "no_framework_request";
    if(s.attemptByte!=1) return "attempt_framework_inconsistent";
    if(!s.requestKnown) return "framework_request_guard_refused";
    if(!requestMatches(s)) return "framework_request_mismatch";
    if(!s.frameworkReturned||s.frameworkByte>1) return "framework_result_unknown";
    return s.frameworkByte?"framework_accepted":"framework_refused";
}
bool writeLine(const char* line) {
    DWORD written=0;const auto size=static_cast<DWORD>(strlen(line));
    if(!WriteFile(logFile,line,size,&written,nullptr)||written!=size) {writeFailed=true;recording=false;return false;}
    return true;
}
bool writeHeader() {
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
#if defined(KT_LEDGE_REPEATABLE)
    constexpr const char* Mode="repeatable-tall-vault";
#elif defined(KT_LEDGE_LIVE_TRIAL) && defined(KT_LEDGE_TALL_PREVIEW)
    constexpr const char* Mode="bounded-tall-vault";
#elif defined(KT_LEDGE_LIVE_TRIAL)
    constexpr const char* Mode="bounded-vault-onto";
#elif defined(KT_LEDGE_TALL_PREVIEW)
    constexpr const char* Mode="dry-tall-preview";
#elif defined(KT_LEDGE_DRY_GUARD)
    constexpr const char* Mode="dry-guide-guard";
#else
    constexpr const char* Mode="observe-only";
#endif
#ifdef KT_LEDGE_DRY_SELECTION
    constexpr const char* Preview=",\"selection_preview\":true";
#else
    constexpr const char* Preview="";
#endif
    char header[512];sprintf_s(header,"{\"event\":\"started\",\"mode\":\"%s\",\"probe\":\"ledge-admission\",\"probe_revision\":4,\"qpc_frequency\":%lld,\"record_limit\":%u,\"max_depth\":%u,\"hook_count\":%u,\"motion_compiled\":false%s}\n",Mode,frequency.QuadPart,MaxRecords,MaxDepth,HookCount,Preview);
    return writeLine(header);
}
void vectorJson(const float* v,char (&out)[128]) {sprintf_s(out,"[%.9g,%.9g,%.9g]",v[0],v[1],v[2]);}
DWORD WINAPI writer(void*) {
    Sample s;char line[2048];
    for(;;) {
        while(queue.pop(s)) {
            if(writeFailed.load()) continue;
            char position[128]="null",direction[128]="null",bounds[128]="null",query[32]="null",
                selection[80]="null",request[80]="null",attemptValue[16]="null",accepted[8]="null",frameworkValue[16]="null",vtableRva[32]="null",componentRva[32]="null";
            if(s.queryInputsKnown) {vectorJson(s.position,position);vectorJson(s.direction,direction);
                sprintf_s(bounds,"[%.9g,%.9g,%.9g,%.9g]",s.bounds[0],s.bounds[1],s.bounds[2],s.bounds[3]);}
            if(s.queryKnown) sprintf_s(query,"%u",s.queryHandle);
            if(s.selectionKnown) sprintf_s(selection,"[%d,%u]",s.selection.actionType,s.selection.handle);
            if(s.requestKnown) sprintf_s(request,"[%d,%u,%u,%u,%u,%u]",s.request.selection.actionType,s.request.selection.handle,
                s.request.flags[0],s.request.flags[1],s.request.flags[2],s.request.flags[3]);
            if(s.actionVtableRva!=0xffffffffu) sprintf_s(vtableRva,"%u",s.actionVtableRva);
            if(s.componentVtableRva!=0xffffffffu) sprintf_s(componentRva,"%u",s.componentVtableRva);
            if(s.attemptReturned) sprintf_s(attemptValue,"%u",s.attemptByte);
            if(s.frameworkReturned) {sprintf_s(frameworkValue,"%u",s.frameworkByte);
                if(s.requestKnown&&s.frameworkByte<=1) strcpy_s(accepted,s.frameworkByte?"true":"false");}
            sprintf_s(line,"{\"event\":\"ledge_admission\",\"transaction\":%llu,\"scope\":\"%s\",\"thread\":%lu,\"entity\":%u,\"depth\":%u,\"begin_qpc\":%lld,\"end_qpc\":%lld,\"query_qpc\":%lld,\"selection_qpc\":%lld,\"framework_qpc\":%lld,\"completed\":%s,\"exceptional\":%s,\"queries\":%u,\"selections\":%u,\"framework_requests\":%u,\"allow_alternate\":%s,\"query_position\":%s,\"query_direction\":%s,\"query_bounds\":%s,\"query_handle\":%s,\"query_return_matches_output\":%s,\"selection\":%s,\"request\":%s,\"request_guard\":\"%s\",\"request_guard_bits\":%u,\"action_vtable_kind\":\"%s\",\"action_vtable_rva\":%s,\"owner_component_vtable_kind\":\"%s\",\"owner_component_vtable_rva\":%s,\"request_matches_selection\":%s,\"attempt_result_byte\":%s,\"framework_result_byte\":%s,\"framework_accepted\":%s,\"stage\":\"%s\"}\n",
                s.transaction,s.scope?"selection_only":"standing_attempt",s.thread,s.entity,s.depth,s.begin,s.end,s.queryTick,s.selectionTick,s.frameworkTick,
                s.completed?"true":"false",s.exceptional?"true":"false",s.queries,s.selections,s.frameworks,s.selections?(s.allowAlternate?"true":"false"):"null",position,direction,bounds,query,
                s.queryReturnMatches?"true":"false",selection,request,GuardNames[static_cast<unsigned>(s.requestGuard)],s.requestGuardBits,vtableKind(s),vtableRva,componentKind(s),componentRva,requestMatches(s)?"true":"false",attemptValue,frameworkValue,accepted,stage(s));
#ifdef KT_LEDGE_REPEATABLE
            if(s.climbGate||s.retryLookup) {
                line[strlen(line)-2]=0;
                if(s.climbGate) {
                    char field[80];sprintf_s(field,",\"climb_gate\":\"%s\"",s.climbGate);
                    strcat_s(line,field);
                }
                if(s.retryLookup)
                    strcat_s(line,s.recoveredLookup?",\"tall_lookup_retry\":\"matched\"":",\"tall_lookup_retry\":\"refused\"");
                strcat_s(line,"}\n");
            }
#endif
            writeLine(line);
        }
        if(stopping.load()) break;Sleep(20);
    }
    sprintf_s(line,"{\"event\":\"stopped\",\"calls\":[%llu,%llu,%llu,%llu],\"transactions_started\":%llu,\"transactions_ended\":%llu,\"owner_refused\":%llu,\"unreadable\":%llu,\"context_overflow\":%llu,\"unlinked_framework\":%llu,\"dropped\":%llu,\"reserved\":%llu,\"committed\":%llu,\"record_limit\":%u,\"truncated\":%s,\"write_failed\":%s}\n",
        calls[0].load(),calls[1].load(),calls[2].load(),calls[3].load(),started.load(),ended.load(),ownerRefused.load(),unreadable.load(),
        contextOverflow.load(),unlinkedFramework.load(),dropped.load(),reserved.load(),committed.load(),MaxRecords,truncated.load()?"true":"false",writeFailed.load()?"true":"false");
    // Append stable guard-specific counters to complete shutdown accounting.
    auto length=strlen(line);line[length-2]=0;
    strcat_s(line,",\"request_guard_counts\":{");
    for(unsigned i=0;i<static_cast<unsigned>(RequestGuard::Count);++i) {
        char field[128];sprintf_s(field,"%s\"%s\":%llu",i?",":"",GuardNames[i],requestGuards[i].load());strcat_s(line,field);
    }
    strcat_s(line,"}");
#ifdef KT_LEDGE_DRY_GUARD
    char field[128];sprintf_s(field,",\"dry_activations\":%u,\"guarded_queries\":%u,\"guard_remains_armed\":true",
        dryActivations.load(),guardedQueries.load());
    strcat_s(line,field);
    strcat_s(line,",\"dry_index_results\":{");
    for(unsigned i=0;i<static_cast<unsigned>(ktledge::DryIndexResult::Count);++i) {
        sprintf_s(field,"%s\"%s\":%u",i?",":"",ktledge::DryIndexNames[i],dryIndexResults[i].load());
        strcat_s(line,field);
    }
    strcat_s(line,"}");
#ifdef KT_LEDGE_DRY_SELECTION
    sprintf_s(field,",\"withheld_selections\":%u",withheldSelections.load());
    strcat_s(line,field);
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
#ifdef KT_LEDGE_REPEATABLE
    sprintf_s(field,",\"repeatable\":true,\"trial_result\":%u,\"trial_framework_forwards\":%u",
        trialResult.load(),trialFrameworkForwards.load());
#else
    sprintf_s(field,",\"trial_armed\":%s,\"trial_result\":%u,\"trial_framework_forwards\":%u",
        trialArmedOnce?"true":"false",trialResult.load(),trialFrameworkForwards.load());
#endif
    strcat_s(line,field);
    sprintf_s(field,",\"trial_ground_samples\":%u,\"trial_used_takeoff\":%s",
        trialGroundSamples.load(),trialUsedTakeoff?"true":"false");
    strcat_s(line,field);
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
    sprintf_s(field,",\"tall_parameter_clones\":%u",tallClones.load());
    strcat_s(line,field);
#endif
#endif
    strcat_s(line,"}\n");
    if(!writeFailed.load()) writeLine(line);FlushFileBuffers(logFile);return 0;
}
bool disableHooks() {
    bool ok=true;
    for(void* target:targets) if(target) {auto r=MH_DisableHook(target);if(r!=MH_OK&&r!=MH_ERROR_DISABLED) ok=false;}
    return ok;
}
DWORD drainWriter() {
    for(unsigned i=0;inFlight.load()&&i<1000;++i) Sleep(1);
    if(inFlight.load()) return ERROR_BUSY;
    stopping=true;
    if(WaitForSingleObject(writerThread,5000)!=WAIT_OBJECT_0) return ERROR_BUSY;
    CloseHandle(writerThread);writerThread=nullptr;CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    return ERROR_SUCCESS;
}
}

extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
    if(attempted.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    DWORD error=ktwin::verifyTarget(gameBase);if(error) return error;
    if(!allPins(gameBase)) return ERROR_REVISION_MISMATCH;
    HMODULE pin=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&KtProbeStart),&pin)) return GetLastError();
    wchar_t path[32768],logPath[32768];
    if(!GetModuleFileNameW(ownModule,path,32768)) return GetLastError();
    auto last=wcsrchr(path,L'\\');if(!last) return ERROR_BAD_PATHNAME;*last=0;
    swprintf_s(logPath,L"%s\\ledge-admission-%lu-%llu.jsonl",path,GetCurrentProcessId(),GetTickCount64());
    logFile=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logFile==INVALID_HANDLE_VALUE) return GetLastError();
    if(!writeHeader()) {CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_WRITE_FAULT;}
    if(MH_Initialize()!=MH_OK) {CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
    void* callbacks[]={reinterpret_cast<void*>(&observedAttempt),reinterpret_cast<void*>(&observedSelection),reinterpret_cast<void*>(&observedQuery),reinterpret_cast<void*>(&observedFramework)
#ifdef KT_LEDGE_DRY_GUARD
        ,reinterpret_cast<void*>(&guardedIndex)
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
        ,reinterpret_cast<void*>(&observedParams)
#endif
    };
    void** originals[]={reinterpret_cast<void**>(&originalAttempt),reinterpret_cast<void**>(&originalSelection),reinterpret_cast<void**>(&originalQuery),reinterpret_cast<void**>(&originalFramework)
#ifdef KT_LEDGE_DRY_GUARD
        ,reinterpret_cast<void**>(&originalIndex)
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
        ,reinterpret_cast<void**>(&originalParams)
#endif
    };
    std::uintptr_t rvas[]={ktledge::AttemptRva,ktledge::SelectionRva,ktledge::QueryRva,ktledge::FrameworkRva
#ifdef KT_LEDGE_DRY_GUARD
        ,ktguide::Hooks[2].rva
#endif
#ifdef KT_LEDGE_TALL_PREVIEW
        ,kttall::ParamsRva
#endif
    };
    unsigned created=0;
    for(;created<HookCount;++created) {
        targets[created]=reinterpret_cast<void*>(gameBase+rvas[created]);
        if(MH_CreateHook(targets[created],callbacks[created],originals[created])!=MH_OK) break;
    }
    if(created!=HookCount) {
        for(unsigned i=0;i<created;++i) MH_RemoveHook(targets[i]);MH_Uninitialize();
        CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;
    }
    recording=true;stopping=false;writerThread=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
    if(!writerThread) {
        recording=false;for(void* t:targets) MH_RemoveHook(t);MH_Uninitialize();
        CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;
    }
    bool enabled=true;
    for(void* t:targets) if(MH_QueueEnableHook(t)!=MH_OK) enabled=false;
    if(enabled) enabled=MH_ApplyQueued()==MH_OK;
    if(!enabled) {
        recording=false;
        // If disable/drain is uncertain leave worker/trampolines alive and
        // support a later Stop retry. Never free possibly executing hook code.
        if(disableHooks()) drainWriter();
        return ERROR_INVALID_FUNCTION;
    }
#ifdef KT_LEDGE_DRY_GUARD
    activationArmed=true;
#endif
    return ERROR_SUCCESS;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
    if(stopBusy.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    if(!writerThread) {stopBusy=false;return ERROR_NOT_READY;}
    recording=false;
#ifdef KT_LEDGE_DRY_GUARD
    // Finish the audit while keeping the query guard resident until game exit.
    activationArmed=false;
#ifdef KT_LEDGE_REPEATABLE
    activeEpoch=0;
#elif defined(KT_LEDGE_LIVE_TRIAL)
    trialDeadline=0;
#endif
#else
    if(!disableHooks()) {stopBusy=false;return ERROR_INVALID_FUNCTION;}
#endif
    const auto result=drainWriter();stopBusy=false;return result;
}
#if defined(KT_LEDGE_LIVE_TRIAL) && !defined(KT_LEDGE_REPEATABLE)
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeArm(void*) {
    if(!recording||!activationArmed||dryActivations.load()!=1) return ERROR_NOT_READY;
    if(trialArmedOnce.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    trialDeadline=GetTickCount64()+30000;
    return ERROR_SUCCESS;
}
#endif
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {ownModule=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
