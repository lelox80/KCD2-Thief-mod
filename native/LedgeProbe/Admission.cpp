// Observe existing native attempts. Never invoke a traversal or change its data.
#include "../StateBridge/WindowsTarget.h"
#include "../StateBridge/AuditQueue.h"
#include "Selection.h"
#include "MinHook.h"
#include <atomic>
#include <cmath>
#include <cwchar>
#include <intrin.h>

namespace {
ktledge::AttemptFn originalAttempt=nullptr;
ktledge::SelectionFn originalSelection=nullptr;
ktledge::QueryFn originalQuery=nullptr;
ktledge::FrameworkFn originalFramework=nullptr;
std::uintptr_t gameBase=0;
HMODULE ownModule=nullptr;
void* targets[4]{};
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
};
struct Frame {std::uintptr_t actor=0;Sample sample;};
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
    if(stack.suppressed||!recording.load(std::memory_order_acquire)) return nullptr;
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
    enqueue(f->sample);++ended;
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
        if(!f) muted=suppressNested();
        result=originalAttempt(opaque1,actor,opaque3,restriction,input);normal=true;
    } __finally {
        if(f) {f->sample.attemptByte=result;f->sample.attemptReturned=normal;leave(f,normal);}
        if(muted) --stack.suppressed;
        --inFlight;
    }
    return result;
}
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
        originalSelection(actor,output,alternate);normal=true;
        if(f) {
            std::uint32_t id=0;
            f->sample.selectionKnown=ownerId(actor,id)&&id==f->sample.entity&&readSelection(output,f->sample.selection);
            if(!f->sample.selectionKnown) ++unreadable;
            f->sample.selectionTick=ticks();
        }
    } __finally {
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
        }
    } __finally {if(f) f->sample.exceptional|=!normal;if(muted) --stack.suppressed;--inFlight;}
    return result;
}
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
        result=originalFramework(framework,holder);normal=true;
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
    char header[512];sprintf_s(header,"{\"event\":\"started\",\"mode\":\"observe-only\",\"probe\":\"ledge-admission\",\"probe_revision\":4,\"qpc_frequency\":%lld,\"record_limit\":%u,\"max_depth\":%u,\"hook_count\":4,\"motion_compiled\":false}\n",frequency.QuadPart,MaxRecords,MaxDepth);
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
    strcat_s(line,"}}\n");
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
    void* callbacks[]={reinterpret_cast<void*>(&observedAttempt),reinterpret_cast<void*>(&observedSelection),reinterpret_cast<void*>(&observedQuery),reinterpret_cast<void*>(&observedFramework)};
    void** originals[]={reinterpret_cast<void**>(&originalAttempt),reinterpret_cast<void**>(&originalSelection),reinterpret_cast<void**>(&originalQuery),reinterpret_cast<void**>(&originalFramework)};
    unsigned created=0;
    for(;created<4;++created) {
        targets[created]=reinterpret_cast<void*>(gameBase+ktledge::Hooks[created].rva);
        if(MH_CreateHook(targets[created],callbacks[created],originals[created])!=MH_OK) break;
    }
    if(created!=4) {
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
    return ERROR_SUCCESS;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
    if(stopBusy.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    if(!writerThread) {stopBusy=false;return ERROR_NOT_READY;}
    recording=false;
    if(!disableHooks()) {stopBusy=false;return ERROR_INVALID_FUNCTION;}
    const auto result=drainWriter();stopBusy=false;return result;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {ownModule=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
