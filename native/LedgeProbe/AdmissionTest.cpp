// Owned-process tests only. No game process, save or installation is opened.
#define KT_LEDGE_FIXTURE 1
#include "Admission.cpp"
#include <cstdlib>
#include <thread>
#include <vector>

void check(bool ok,const char* message) {if(!ok) {fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
void ptr(unsigned char* p,std::size_t offset,std::uintptr_t value) {memcpy(p+offset,&value,sizeof value);}
void id(unsigned char* p,std::size_t offset,std::uint32_t value) {memcpy(p+offset,&value,sizeof value);}
struct Owner {
    alignas(8) unsigned char actor[0x288]{},controller[0x20]{},entity[0x20]{},component[0x80]{};
    explicit Owner(std::uint32_t value) {
        ptr(actor,0,gameBase+ktretail::PlayerVtable);ptr(controller,0,gameBase+ktretail::ControllerVtable);
        ptr(entity,0,gameBase+ktretail::EntityVtable);ptr(actor,0x180,reinterpret_cast<std::uintptr_t>(controller));
        ptr(actor,0x38,reinterpret_cast<std::uintptr_t>(entity));ptr(controller,8,reinterpret_cast<std::uintptr_t>(actor));
        id(actor,0x30,value);id(entity,0xc,value);
        ptr(actor,ktledge::PlayerActionActorOffset,reinterpret_cast<std::uintptr_t>(component));
        ptr(component,0,gameBase+ktledge::ActionActorVtableRva);
        ptr(component,ktledge::ActionActorPlayerOffset,reinterpret_cast<std::uintptr_t>(actor));
    }
};
struct FrameworkStorage {
    unsigned char action[0x80]{},framework[0x10]{},core[0x60]{},resource[0x10]{};
    void* holder=nullptr;
    FrameworkStorage(void* actor,ktledge::Selection selection) {
        const auto component=*reinterpret_cast<std::uintptr_t*>(static_cast<unsigned char*>(actor)+ktledge::PlayerActionActorOffset);
        ptr(action,0,gameBase+ktledge::LedgeActionVtableRva);ptr(action,0x58,component);
        const ktledge::Request request{selection,{1,0,0,0}};memcpy(action+0x68,&request,sizeof request);
        ptr(framework,8,reinterpret_cast<std::uintptr_t>(core));ptr(core,0x50,reinterpret_cast<std::uintptr_t>(resource));
        ptr(resource,8,component);holder=action;
    }
};
unsigned char managerStorage[8]{};
void* manager=managerStorage;
std::atomic<unsigned> forwarded[4]{};
unsigned testMode=0;
thread_local bool stackArgumentsSeen=false,opaqueArgumentsSeen=false,holderSeen=false;
constexpr std::uintptr_t Opaque1=0x1020304050607080ull,Opaque3=0x8877665544332211ull,Restriction=0x0abc1234ffffull;
std::uint32_t alternateOutput=0;
#ifdef KT_LEDGE_TALL_PREVIEW
void* __fastcall targetParams(void* bank,std::uint32_t) {return bank;}
#endif
__declspec(noinline) std::uint32_t* __fastcall targetQuery(void* m,std::uint32_t* output,const float* position,
                                                        const float* direction,float a,float b,float c,float d) {
    ++forwarded[2];
    stackArgumentsSeen=m==manager&&position[0]==3158.25f&&direction[1]==1&&a==1.6f&&b==0.7f&&c==0.8f&&d==0.5f;
    if(testMode==7) RaiseException(0xe04b7401,0,0,nullptr);
    *output=testMode==1?0xffffffffu:0x02d10000u;
#ifdef KT_LEDGE_DRY_GUARD
    if(testMode==15||testMode==16) *output=(ktledge::DryGuideObject<<16)|(testMode==16?1u:0u);
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
    if(testMode==17) *output=ktvault::Handle;
#ifdef KT_LEDGE_REPEATABLE
    if(testMode==18) *output=ktvault::Walls[1].handle;
    if(testMode>=19&&testMode<=22) {
        const bool retry=position==ktvault::TallLookupAnchor&&direction==ktvault::TallLookupDirection;
        if(testMode==21) *output=0x02d10000u;
        else if(!retry) *output=ktledge::NoGuide;
        else if(testMode==19) *output=ktvault::Handle;
        else if(testMode==20) *output=0x02d10000u;
        else *output=ktvault::Handle|1u;
    }
#endif
#endif
    return testMode==6?&alternateOutput:output;
}
__declspec(noinline) void __fastcall targetSelection(void* actor,ktledge::Selection* output,bool alternate) {
    ++forwarded[1];check(alternate,"alternate flag forwarded");
    float position[]={3158.25f,542.5f,46.75f},direction[]={0,1,0};std::uint32_t handle=0xffffffffu;
#if defined(KT_LEDGE_LIVE_TRIAL) && defined(KT_LEDGE_TALL_PREVIEW)
    if(testMode==17) {direction[0]=ktvault::Facing.x;direction[1]=ktvault::Facing.y;}
#endif
#ifdef KT_LEDGE_REPEATABLE
    if(testMode>=19&&testMode<=22) {
        // Recorded lateral miss at a distance where the center worked.
        const float recorded[]={2896.24243f,793.874939f,65.3354263f};
        memcpy(position,recorded,sizeof recorded);
        memcpy(direction,ktvault::TallLookupDirection,sizeof direction);
    }
#endif
    ktledge::QueryFn volatile query=&targetQuery;
    query(manager,&handle,position,direction,1.6f,0.7f,0.8f,0.5f);
    if(testMode==8) throw 19;
    output->actionType=testMode==2||handle==0xffffffffu?-1:2;output->handle=handle;
#ifdef KT_LEDGE_LIVE_TRIAL
    const bool liveTall=(testMode==17||testMode==19)&&handle==ktvault::Handle;
    if(liveTall) output->actionType=4;
#ifdef KT_LEDGE_REPEATABLE
    if(testMode==18&&handle==ktvault::Walls[1].handle) output->actionType=4;
#endif
#endif
#if defined(KT_LEDGE_LIVE_TRIAL) && defined(KT_LEDGE_TALL_PREVIEW)
    if(liveTall) {
        unsigned char stock[kttall::ParamsSize]{};
        const float max=1.5f;
        memcpy(stock+kttall::VaultOntoMax,&max,sizeof max);
        observedParams(stock,0);
    }
#endif
    if(testMode==12) { // Nested foreign owner must never borrow this transaction.
        ktledge::Selection other;ktledge::SelectionFn volatile nested=&targetSelection;
        const auto prior=testMode;testMode=0;nested(nullptr,&other,true);testMode=prior;
    }
    (void)actor;
}
__declspec(noinline) std::uint8_t __fastcall targetFramework(void* framework,void* holder) {
    ++forwarded[3];
    holderSeen=framework!=nullptr&&holder!=nullptr&&*static_cast<void**>(holder)!=nullptr;
    // Simulate host lifetime ending during admission. Post-call reads of action
    // memory would now lose the request/vtable, while the pre-call copy survives.
    memset(*static_cast<void**>(holder),0,0x80);
    if(testMode==9) RaiseException(0xe04b7401,0,0,nullptr);
    return testMode==4?0:testMode==5?7:1;
}
__declspec(noinline) std::uint8_t __fastcall targetAttempt(std::uintptr_t a,void* actor,std::uintptr_t c,
                                                        std::uintptr_t restriction,void* input) {
    ++forwarded[0];opaqueArgumentsSeen=a==Opaque1&&c==Opaque3&&restriction==Restriction&&input==actor;
    if(testMode==10) throw 23;
    ktledge::Selection selection;ktledge::SelectionFn volatile select=&targetSelection;
    select(actor,&selection,true);
    if(selection.actionType!=-1&&testMode!=3) {
        FrameworkStorage storage(actor,selection);
        if(testMode==14)ptr(storage.action,0,gameBase+ktledge::LedgeActionSecondary10VtableRva);
        if(testMode==11) id(storage.action,0x68,9); // action differs from selected pair
        ktledge::FrameworkFn volatile request=&targetFramework;request(storage.framework,&storage.holder);
    }
    return testMode==13?9:selection.actionType!=-1?1:0;
}
void drain() {Sample s;while(queue.pop(s)) {}}
void resetQueue() {drain();reserved=0;committed=0;dropped=0;truncated=false;writeFailed=false;recording=true;stopping=false;}
Sample run(Owner& owner,unsigned mode) {
    resetQueue();testMode=mode;fixtureCallers=true;
    ktledge::AttemptFn volatile attempt=&targetAttempt;
    const auto value=attempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);
    check(value==(mode==13?9:mode==1||mode==2||mode==15||mode==16?0:1),"host attempt byte preserved");
    check(inFlight==0&&stack.depth==0&&stack.suppressed==0,"transaction stack drained");
    Sample s;check(queue.pop(s)&&!queue.pop(s),"one complete outer transaction");
    check(s.scope==0&&s.entity==73&&s.depth==1&&s.queries==1&&s.selections==1,"query and selection nested in exact actor transaction");
    check(stackArgumentsSeen&&opaqueArgumentsSeen,"opaque registers and all float stack arguments unchanged");
    return s;
}
bool catchSeh(Owner& owner,unsigned mode) {
    testMode=mode;
    __try {observedAttempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);return false;}
    __except(GetExceptionCode()==0xe04b7401?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return true;}
}
int main() {
#ifdef KT_LEDGE_TALL_PREVIEW
    {
        unsigned char block[kttall::ParamsSize]{};
        kttall::ParamBlock copy{};
        const float stock=1.5f;
        memcpy(block+kttall::VaultOntoMax,&stock,sizeof stock);
        check(guardedRead(block,copy)&&kttall::extendHeight(copy),"stock height copied and extended");
        check(!kttall::extendHeight(copy),"unexpected maximum refused");
        memcpy(reinterpret_cast<unsigned char*>(copy)+kttall::VaultOntoMax,&stock,sizeof stock);
        check(!memcmp(block,copy,sizeof block),"only VaultOnto allowed maximum changed");
        originalParams=targetParams;
        fixtureCallers=true;
        check(observedParams(block,0)==block,"unscoped lookup preserves stock pointer");
        stack.depth=1;
        stack.frame[0].previewActive=true;
        check(observedParams(block,0)==block,"base lookup preserves stock pointer");
        stack.frame[0].tallQuery=true;
        const auto scoped=observedParams(block,0);
        check(scoped==stack.frame[0].tallParams&&tallClones==1,"fixture scope receives the temporary copy");
        float original=0;
        memcpy(&original,block+kttall::VaultOntoMax,sizeof original);
        check(original==stock,"stock bank untouched");
        stack=FrameStack{};
        fixtureCallers=false;
    }
#endif
    check(KtProbeStart(nullptr)==ERROR_MOD_NOT_FOUND,"absent game fails closed");
    check(KtProbeStart(nullptr)==ERROR_ALREADY_INITIALIZED,"one initialization attempt");
    check(KtProbeStop(nullptr)==ERROR_NOT_READY,"stop before start");
    gameBase=reinterpret_cast<std::uintptr_t>(&manager)-ktledge::ManagerGlobalRva;
#ifdef KT_LEDGE_DRY_GUARD
    {
        std::vector<unsigned char> header(0x60029),ledges(ktledge::DryLedgeCount*16),markers(ktledge::DryMarkerCount*28);
        FILE* source=nullptr;
        check(!fopen_s(&source,"dry-member-fixture.bin","rb"),"pinned member fixture available");
        check(fread(ledges.data(),1,ledges.size(),source)==ledges.size()&&
            fread(markers.data(),1,markers.size(),source)==markers.size()&&fgetc(source)==EOF,
            "pinned member fixture read exactly");
        fclose(source);
        ptr(header.data(),0,reinterpret_cast<std::uintptr_t>(ledges.data()));
        ptr(header.data(),8,reinterpret_cast<std::uintptr_t>(markers.data()));
        id(header.data(),0x10,ktledge::DryLedgeCount);id(header.data(),0x14,ktledge::DryMarkerCount);
        using IndexResult=ktledge::DryIndexResult;
        check(enableOwnedGuide(header.data())==IndexResult::Manager,"foreign manager cannot enable fixture");
        manager=header.data();
        header[0x60028]=1;
        check(enableOwnedGuide(manager)==IndexResult::Editor,"editor manager cannot enable fixture");
        header[0x60028]=0;
        markers.back()^=1;
        check(ktledge::checkDryMember(ledges.data(),ktledge::DryLedgeCount,markers.data(),ktledge::DryMarkerCount)==IndexResult::Matched,
            "native marker padding does not affect identity");
        markers[0]^=1;
        check(enableOwnedGuide(manager)==IndexResult::Markers,"changed marker data refuses fixture");
        markers[0]^=1;
        const auto before=ledges;
        check(enableOwnedGuide(manager)==IndexResult::Matched&&enableOwnedGuide(manager)==IndexResult::Tail,
            "owned inactive fixture enables once");
#ifdef KT_LEDGE_REPEATABLE
        check(enableOwnedGuide(manager,true)==IndexResult::Matched,"same enabled member can rebuild");
        activationArmed=true;
        indexEpoch=activeEpoch=1;
        check(activeGuide(1)&&!activeGuide(2),"only the indexed member generation is active");
        markers[(ktledge::DryMarkerCount-1)*28]^=1;
        check(!activeGuide(1)&&enableOwnedGuide(manager,true)==IndexResult::Markers,
            "changed active marker is refused");
        markers[(ktledge::DryMarkerCount-1)*28]^=1;
#endif
        for(unsigned index=0;index<ktledge::DryGuideCount;++index) {
            const auto offset=(ktledge::DryGuideObject+index)*ktledge::LedgeSize+8;
            std::uint16_t flags=0;
            memcpy(&flags,ledges.data()+offset,sizeof flags);
            check(flags==ktledge::DrySideFlags,"exact 16-bit side flags");
            memcpy(ledges.data()+offset,before.data()+offset,sizeof flags);
        }
        check(ledges==before,"activation changes only owned guide side flags");
        manager=managerStorage;
    }
#endif
    Owner owner(73);std::uint32_t entity=0;
    check(ownerId(owner.actor,entity)&&entity==73,"complete actor owner chain");
    id(owner.entity,0xc,74);check(!ownerId(owner.actor,entity),"reused/mismatched identity refused");id(owner.entity,0xc,73);
    ktledge::Selection pair;check(!readSelection(nullptr,pair),"invalid selection guarded");
    std::uint32_t handle=0;check(!readHandle(nullptr,handle),"invalid query output guarded");
    Sample input;const float p[]={1,2,3},d[]={0,1,0},b[]={1,2,3,4};
    check(queryInputs(manager,p,d,b,input)&&!queryInputs(nullptr,p,d,b,input),"manager identity checked without engine calls");
    check(!queryInputs(manager,nullptr,d,b,input),"query vector invalid read guarded");
    FrameworkStorage storage(owner.actor,{2,0x02d10000});Frame f;f.actor=reinterpret_cast<std::uintptr_t>(owner.actor);f.sample.entity=73;
    check(frameworkRequest(storage.framework,&storage.holder,&f)==RequestGuard::Matched,"action/framework/actor ownership checked before original");
    check(frameworkRequest(nullptr,&storage.holder,&f)==RequestGuard::NullFramework,"null framework distinguished");
    id(storage.action,0x68,8);check(frameworkRequest(storage.framework,&storage.holder,&f)==RequestGuard::Matched&&f.sample.request.selection.actionType==8,"exact 12-byte copied request");

    // Source-backed owner relation: canonical C_Player -> C_ActionActor ->
    // canonical back-pointer. Action and framework both own that component.
    auto guard=[&](void* framework,void* holder,RequestGuard expected,unsigned bits) {
        const auto actual=frameworkRequest(framework,holder,&f);
        check(actual==expected&&f.sample.requestGuardBits==bits,"exact request guard and passed-predicate bits");
    };
    guard(storage.framework,&storage.holder,RequestGuard::Matched,2047);
    check(f.sample.componentVtableRva==ktledge::ActionActorVtableRva&&!strcmp(componentKind(f.sample),"action_actor"),"separate action actor identity copied as target-relative class only");
    guard(storage.framework,nullptr,RequestGuard::NullHolder,0);
    check(!f.sample.actionVtableRead&&!f.sample.componentVtableRead&&f.sample.actionVtableRva==0xffffffffu&&
        f.sample.componentVtableRva==0xffffffffu&&!strcmp(vtableKind(f.sample),"unobserved")&&!strcmp(componentKind(f.sample),"unobserved"),
        "later absent holder cannot inherit prior action/component identity");
    guard(storage.framework,reinterpret_cast<void*>(1),RequestGuard::UnreadableHolder,0);
    void* empty=nullptr;guard(storage.framework,&empty,RequestGuard::EmptyHolder,0);
    void* inaccessible=reinterpret_cast<void*>(1);guard(storage.framework,&inaccessible,RequestGuard::UnreadableVtable,1);
    ptr(storage.action,0,gameBase+ktledge::LedgeActionSecondary50VtableRva);
    guard(storage.framework,&storage.holder,RequestGuard::VtableMismatch,1);
    check(!strcmp(vtableKind(f.sample),"ledge_secondary_50"),"secondary interface diagnosed without normalization");
    ptr(storage.action,0,0x1234);guard(storage.framework,&storage.holder,RequestGuard::VtableMismatch,1);
    check(!strcmp(vtableKind(f.sample),"outside_target")&&f.sample.actionVtableRva==0xffffffffu,"external pointer never retained");
    ptr(storage.action,0,gameBase+0x1000);guard(storage.framework,&storage.holder,RequestGuard::VtableMismatch,1);
    check(!strcmp(vtableKind(f.sample),"other_target_rva")&&f.sample.actionVtableRva==0x1000,"only target-relative code identity retained");
    ptr(storage.action,0,gameBase+ktledge::LedgeActionVtableRva);
    id(owner.entity,0xc,74);guard(storage.framework,&storage.holder,RequestGuard::CanonicalOwnerInvalid,3);id(owner.entity,0xc,73);
    f.sample.entity=74;guard(storage.framework,&storage.holder,RequestGuard::CanonicalEntityMismatch,3);f.sample.entity=73;
    ptr(owner.actor,ktledge::PlayerActionActorOffset,0);guard(storage.framework,&storage.holder,RequestGuard::NullComponent,7);
    ptr(owner.actor,ktledge::PlayerActionActorOffset,1);guard(storage.framework,&storage.holder,RequestGuard::UnreadableComponentVtable,15);
    ptr(owner.actor,ktledge::PlayerActionActorOffset,reinterpret_cast<std::uintptr_t>(owner.component));
    ptr(owner.component,0,gameBase+0x1000);guard(storage.framework,&storage.holder,RequestGuard::ComponentVtableMismatch,15);
    check(!strcmp(componentKind(f.sample),"other_target_rva")&&f.sample.componentVtableRva==0x1000,"mismatched component class is separately diagnosed");
    ptr(owner.component,0,gameBase+ktledge::ActionActorVtableRva);
    ptr(owner.component,ktledge::ActionActorPlayerOffset,0);guard(storage.framework,&storage.holder,RequestGuard::ComponentPlayerMismatch,31);
    ptr(owner.component,ktledge::ActionActorPlayerOffset,reinterpret_cast<std::uintptr_t>(owner.actor));
    ptr(storage.action,0x58,0);guard(storage.framework,&storage.holder,RequestGuard::NullActionOwner,63);
    // v17's incorrect direct canonical pointer must now be refused, never used
    // as a fallback. A foreign component with the right class is refused too.
    ptr(storage.action,0x58,reinterpret_cast<std::uintptr_t>(owner.actor));
    guard(storage.framework,&storage.holder,RequestGuard::ActionOwnerMismatch,63);
    Owner foreign(73);ptr(storage.action,0x58,reinterpret_cast<std::uintptr_t>(foreign.component));
    guard(storage.framework,&storage.holder,RequestGuard::ActionOwnerMismatch,63);
    ptr(storage.action,0x58,reinterpret_cast<std::uintptr_t>(owner.component));
    guard(nullptr,&storage.holder,RequestGuard::NullFramework,127);
    guard(reinterpret_cast<void*>(1),&storage.holder,RequestGuard::UnreadableCore,127);
    ptr(storage.framework,8,0);guard(storage.framework,&storage.holder,RequestGuard::NullCore,127);
    ptr(storage.framework,8,1);guard(storage.framework,&storage.holder,RequestGuard::UnreadableResource,255);
    ptr(storage.framework,8,reinterpret_cast<std::uintptr_t>(storage.core));
    ptr(storage.core,0x50,0);guard(storage.framework,&storage.holder,RequestGuard::NullResource,255);
    ptr(storage.core,0x50,1);guard(storage.framework,&storage.holder,RequestGuard::UnreadableFrameworkActor,511);
    ptr(storage.core,0x50,reinterpret_cast<std::uintptr_t>(storage.resource));
    ptr(storage.resource,8,reinterpret_cast<std::uintptr_t>(owner.actor));guard(storage.framework,&storage.holder,RequestGuard::FrameworkActorMismatch,511);
    ptr(storage.resource,8,reinterpret_cast<std::uintptr_t>(owner.component));
    SYSTEM_INFO systemInfo;GetSystemInfo(&systemInfo);const auto page=systemInfo.dwPageSize;
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(memory!=nullptr,"owned fault region allocated");DWORD oldProtection=0;
    check(VirtualProtect(memory+page,page,PAGE_NOACCESS,&oldProtection)!=0,"owned fault page protected");
    auto shortAction=memory+page-0x58;ptr(shortAction,0,gameBase+ktledge::LedgeActionVtableRva);
    void* shortHolder=shortAction;guard(storage.framework,&shortHolder,RequestGuard::UnreadableActionOwner,63);
    shortAction=memory+page-0x70;ptr(shortAction,0,gameBase+ktledge::LedgeActionVtableRva);
    ptr(shortAction,0x58,reinterpret_cast<std::uintptr_t>(owner.component));shortHolder=shortAction;
    guard(storage.framework,&shortHolder,RequestGuard::UnreadableRequest,1023);
    auto shortComponent=memory+page-ktledge::ActionActorPlayerOffset;
    ptr(shortComponent,0,gameBase+ktledge::ActionActorVtableRva);
    ptr(owner.actor,ktledge::PlayerActionActorOffset,reinterpret_cast<std::uintptr_t>(shortComponent));
    guard(storage.framework,&storage.holder,RequestGuard::UnreadableComponentPlayer,31);
    ptr(owner.actor,ktledge::PlayerActionActorOffset,reinterpret_cast<std::uintptr_t>(owner.component));
    // Full canonical chain remains readable while its late component link is
    // on the protected next page; this tests a true link fault separately.
    auto shortPlayer=memory+page-ktledge::PlayerActionActorOffset;
    ptr(shortPlayer,0,gameBase+ktretail::PlayerVtable);ptr(shortPlayer,0x180,reinterpret_cast<std::uintptr_t>(owner.controller));
    ptr(shortPlayer,0x38,reinterpret_cast<std::uintptr_t>(owner.entity));id(shortPlayer,0x30,73);
    ptr(owner.controller,8,reinterpret_cast<std::uintptr_t>(shortPlayer));f.actor=reinterpret_cast<std::uintptr_t>(shortPlayer);
    guard(storage.framework,&storage.holder,RequestGuard::UnreadableComponent,7);
    f.actor=reinterpret_cast<std::uintptr_t>(owner.actor);ptr(owner.controller,8,f.actor);
    check(VirtualFree(memory,0,MEM_RELEASE)!=0,"owned fault pages released");
    guard(storage.framework,&storage.holder,RequestGuard::Matched,2047);
    check(!guardUnreadable(RequestGuard::VtableMismatch)&&guardUnreadable(RequestGuard::UnreadableRequest),"layout refusal distinct from unreadable memory");

    check(MH_Initialize()==MH_OK,"MinHook initialized in owned process");
    void* targetsLocal[]={reinterpret_cast<void*>(&targetAttempt),reinterpret_cast<void*>(&targetSelection),reinterpret_cast<void*>(&targetQuery),reinterpret_cast<void*>(&targetFramework)};
    void* hooks[]={reinterpret_cast<void*>(&observedAttempt),reinterpret_cast<void*>(&observedSelection),reinterpret_cast<void*>(&observedQuery),reinterpret_cast<void*>(&observedFramework)};
    void** originals[]={reinterpret_cast<void**>(&originalAttempt),reinterpret_cast<void**>(&originalSelection),reinterpret_cast<void**>(&originalQuery),reinterpret_cast<void**>(&originalFramework)};
    for(unsigned i=0;i<4;++i)check(MH_CreateHook(targetsLocal[i],hooks[i],originals[i])==MH_OK&&MH_QueueEnableHook(targetsLocal[i])==MH_OK,"owned hook created/queued");
    check(MH_ApplyQueued()==MH_OK,"four hooks enabled together");
    auto sample=run(owner,0);
    const Sample acceptedNative=sample;
    check(!strcmp(stage(sample),"framework_accepted")&&sample.frameworkByte==1&&requestMatches(sample)&&holderSeen,"actual framework acceptance distinguished from selection");
    check(sample.queryTick<=sample.selectionTick&&sample.selectionTick<=sample.frameworkTick&&sample.frameworkTick<=sample.end,"nested observation ordering");
    const Sample missingNative=run(owner,1);
    check(!strcmp(stage(missingNative),"no_query_handle"),"raw manager miss distinguished");
#ifdef KT_LEDGE_DRY_GUARD
#ifdef KT_LEDGE_DRY_SELECTION
    dryActivations=1;
#endif
    for(unsigned mode=15;mode<=16;++mode) {
        const auto blocked=run(owner,mode);
        const auto expected=(ktledge::DryGuideObject<<16)|(mode-15);
#ifdef KT_LEDGE_DRY_SELECTION
        check(blocked.queryHandle==expected&&blocked.selection.handle==expected&&
            blocked.selection.actionType==2&&!blocked.frameworks&&!blocked.attemptByte,
            "native choice observed and withheld before the attempt can create an action");
#else
        check(blocked.queryHandle==expected&&
            blocked.selection.handle==ktledge::NoGuide&&blocked.selection.actionType==-1&&!blocked.frameworks,
            "either side is observed and withheld before selection");
#endif
    }
#ifdef KT_LEDGE_DRY_SELECTION
    check(withheldSelections==2,"both preview results were withheld");
#endif
    recording=false;
    fixtureCallers=false;
    testMode=16;
    const float position[]={3158.25f,542.5f,46.75f},direction[]={0,1,0};
    std::uint32_t guarded=0;
    ktledge::QueryFn volatile dryQuery=&targetQuery;
    check(dryQuery(manager,&guarded,position,direction,1.6f,0.7f,0.8f,0.5f)==&guarded&&guarded==ktledge::NoGuide,
        "guard remains active outside observation and preserves output pointer");
#endif
#ifdef KT_LEDGE_LIVE_TRIAL
    {
        resetQueue();
        fixtureCallers=true;
        fixtureClearance=true;
        activationArmed=true;
        testMode=17;
        unsigned char idle[]={0,0},jump[]={1,0};
        ktledge::AttemptFn volatile trial=&targetAttempt;
#ifdef KT_LEDGE_REPEATABLE
        const auto attempt=[&](unsigned char* input) {return trial(Opaque1,owner.actor,Opaque3,Restriction,input);};
        check(attempt(jump)==0,"held Jump entering a level does not climb");
        check(attempt(idle)==0&&attempt(jump)==1&&trialFrameworkForwards==1,
            "first fresh Jump reaches the original framework");
        check(attempt(jump)==0&&attempt(idle)==0&&attempt(jump)==0&&trialFrameworkForwards==1,
            "held Jump and immediate reentry are refused");
        recording=false;
        nextTrial=0;
        check(attempt(idle)==0&&attempt(jump)==1&&trialFrameworkForwards==2,
            "fresh Jump can climb again after audit stops");
        activeEpoch=0;
        nextTrial=0;
        check(attempt(idle)==0&&attempt(jump)==0&&trialFrameworkForwards==2,
            "invalidated index refuses climb");
        indexEpoch=activeEpoch=2;
        check(attempt(jump)==0&&attempt(idle)==0&&attempt(jump)==1&&trialFrameworkForwards==3,
            "matched replacement index requires a fresh Jump");
        testMode=18;
        nextTrial=0;
        const auto tallBefore=tallClones.load();
        check(attempt(idle)==0&&attempt(jump)==1&&trialFrameworkForwards==4&&tallClones==tallBefore,
            "second wall uses stock height and forwards its own exact request");
        testMode=19;
        nextTrial=0;
        const auto queriesBefore=forwarded[2].load();
        check(attempt(idle)==0&&attempt(jump)==1&&trialFrameworkForwards==5&&forwarded[2]==queriesBefore+4,
            "one native retry per callback can recover a previously missed tall wall");
        for(unsigned mode:{20u,22u}) {
            testMode=mode;
            nextTrial=0;
            check(attempt(idle)==0&&attempt(jump)==0&&trialFrameworkForwards==5,
                "retry discards foreign guides and the wrong side of the owned guide");
        }
        testMode=21;
        const auto beforeStock=forwarded[2].load();
        check(attempt(idle)==1&&attempt(jump)==1&&trialFrameworkForwards==5&&forwarded[2]==beforeStock+2,
            "stock first lookup keeps its result and skips retry");
        const float farMiss[]={2896.55762f,794.643311f,65.3663635f};
        const float sideMiss[]={2896.24243f,793.874939f,65.3354263f};
        const float closeMiss[]={2895.81836f,794.879333f,65.4591522f};
        const float tooFar[]={2897.1f,794.64f,65.37f};
        const float facingAway[]={-ktvault::Facing.x,-ktvault::Facing.y,0};
        check(ktvault::tallRetryInRange(farMiss,ktvault::TallLookupDirection)&&
              ktvault::tallRetryInRange(sideMiss,ktvault::TallLookupDirection)&&
              ktvault::tallRetryInRange(closeMiss,ktvault::TallLookupDirection),
              "far, lateral and close misses reach the bounded retry");
        check(!ktvault::tallRetryInRange(tooFar,ktvault::TallLookupDirection)&&
              !ktvault::tallRetryInRange(sideMiss,facingAway),"distant and reverse lookups never retry");
        for(const auto& wall:ktvault::Walls) {
            const auto facing=wall.facing;
            const float straight[]={facing.x,facing.y,0},away[]={-facing.x,-facing.y,0};
            const auto start=wall.corridor.approach;
            const auto lip=wall.lip;
            ktcapsule::Corridor route;
            const ktcapsule::Vector closer{lip.x-facing.x*0.5f,lip.y-facing.y*0.5f,start.z};
            const ktcapsule::Vector farther{lip.x-facing.x*1.2f,lip.y-facing.y*1.2f,start.z};
            const ktcapsule::Vector outside{closer.x+facing.y*0.6f,closer.y-facing.x*0.6f,start.z};
            check(ktvault::approachRoute(wall,closer,straight,route)&&
                  ktvault::approachRoute(wall,farther,straight,route),"closer and farther approaches permitted");
            check(!ktvault::approachRoute(wall,outside,straight,route)&&
                  !ktvault::approachRoute(wall,closer,away,route),"off-edge and reverse approaches refused");
            const ktcapsule::Vector side{closer.x+facing.y*0.2f,closer.y-facing.x*0.2f,start.z};
            check(ktvault::approachRoute(wall,side,straight,route)&&
                  std::abs((route.landing.x-lip.x)*facing.y-(route.landing.y-lip.y)*facing.x-0.2f)<0.001f,
                  "landing follows the aimed lane within the authored strip");
        }
#else
        check(KtProbeArm(nullptr)==0&&KtProbeArm(nullptr)==ERROR_ALREADY_INITIALIZED,"one explicit arm");
        check(trial(Opaque1,owner.actor,Opaque3,Restriction,idle)==0&&trialDeadline!=0,"idle cannot consume arm");
        check(trial(Opaque1,owner.actor,Opaque3,Restriction,jump)==1&&trialFrameworkForwards==1&&trialDeadline==0,
            "exact live selection forwarded through original framework once");
        check(trial(Opaque1,owner.actor,Opaque3,Restriction,jump)==0&&trialFrameworkForwards==1,
            "subsequent jump withheld");
#endif
        const ktcapsule::Vector previous{10,20,30};
        ktvault::Contact contact{{10.01f,20.01f,30.03f},false,true};
        check(ktvault::immediateTakeoff(contact,previous,100)&&
              !ktvault::immediateTakeoff(contact,previous,101),"takeoff requires fresh ground contact");
        contact.position.z=30.06f;
        check(!ktvault::immediateTakeoff(contact,previous,1),"midair rise refused");
        fixtureClearance=false;
#ifndef KT_LEDGE_REPEATABLE
        activationArmed=false;
#endif
    }
#endif
    check(!strcmp(stage(run(owner,2)),"selection_refused"),"manager hit rejected by selector distinguished");
    check(!strcmp(stage(run(owner,3)),"no_framework_request"),"later refusal before framework distinguished");
    check(!strcmp(stage(run(owner,4)),"framework_refused"),"false framework result preserved");
    check(!strcmp(stage(run(owner,5)),"framework_result_unknown"),"nonboolean return remains unknown");
    check(!strcmp(stage(run(owner,6)),"query_unknown"),"query return pointer preserved and mismatch remains unknown");
    check(!strcmp(stage(run(owner,11)),"framework_request_mismatch"),"different action cannot inherit selected candidate");
    const auto priorUnreadable=unreadable.load();
    const auto secondary=run(owner,14);
    check(!strcmp(stage(secondary),"framework_request_guard_refused")&&secondary.requestGuard==RequestGuard::VtableMismatch&&
        secondary.actionVtableRva==ktledge::LedgeActionSecondary10VtableRva&&!strcmp(vtableKind(secondary),"ledge_secondary_10")&&
        secondary.frameworkByte==1&&!secondary.requestKnown&&unreadable==priorUnreadable,"known secondary vtable classified without weakening guard or inventing request");
    check(!strcmp(stage(run(owner,13)),"attempt_result_unknown"),"nonboolean outer result is preserved and remains unknown");
    resetQueue();testMode=0;fixtureCallers=true;
    ktledge::SelectionFn volatile select=&targetSelection;select(owner.actor,&pair,true);
    check(queue.pop(sample)&&sample.scope==1&&!strcmp(stage(sample),"selected_only"),"standalone selection never labelled admitted");

    // Proved caller filter still applies in production; fixture override can be
    // disabled to exercise the real mismatch path against owned call sites.
    resetQueue();fixtureCallers=false;ktledge::AttemptFn volatile attempt=&targetAttempt;
    attempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);
    check(!queue.pop(sample),"unproved caller excluded");
    fixtureCallers=true;
    resetQueue();testMode=12;attempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);
    check(queue.pop(sample)&&sample.contextSuppressed&&!strcmp(stage(sample),"context_unobserved"),"invalid nested actor cannot pollute parent query/selection");
    check(stack.depth==0&&stack.suppressed==0,"nested suppression restored");

    for(unsigned mode:{7u,9u}) {
        resetQueue();check(catchSeh(owner,mode),"host SEH propagated");
        check(inFlight==0&&stack.depth==0&&stack.suppressed==0,"SEH clears counters and scoped actor");
        check(queue.pop(sample)&&sample.exceptional&&!strcmp(stage(sample),"host_exception"),"SEH audit cannot become acceptance");
    }
    for(unsigned mode:{8u,10u}) {
        resetQueue();testMode=mode;bool caught=false;
        try {attempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);}catch(int value) {caught=value==(mode==8?19:23);}
        check(caught&&inFlight==0&&stack.depth==0&&stack.suppressed==0,"C++ unwind and caller state preserved");
        check(queue.pop(sample)&&sample.exceptional,"C++ exception retained");
    }
    testMode=0;resetQueue();fixtureCallers=true;
    Frame* frames[MaxDepth]{};for(unsigned i=0;i<MaxDepth;++i)frames[i]=enter(owner.actor,0);
    check(!enter(owner.actor,0)&&!recording&&contextOverflow==1,"TLS depth bounded");
    for(unsigned i=MaxDepth;i>0;--i)leave(frames[i-1],true);
    check(stack.depth==0,"bounded frames drain");resetQueue();
    std::vector<std::thread> threads;
    for(unsigned i=0;i<4;++i)threads.emplace_back([&owner] {
        fixtureCallers=true;ktledge::AttemptFn volatile call=&targetAttempt;
        for(unsigned j=0;j<100;++j)call(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);
        check(stack.depth==0&&stack.suppressed==0,"thread-local transactions drain");
    });
    for(auto& thread:threads)thread.join();
    unsigned count=0;while(queue.pop(sample)) {++count;check(!strcmp(stage(sample),"framework_accepted"),"parallel association remains local");}
    check(count==400&&dropped==0,"parallel transactions fully accounted");
    for(unsigned i=0;i<4;++i)check(MH_QueueDisableHook(targetsLocal[i])==MH_OK,"disable queued");
    check(MH_ApplyQueued()==MH_OK,"all original sites restored");
    const auto priorCalls=calls[0].load();attempt(Opaque1,owner.actor,Opaque3,Restriction,owner.actor);
    check(calls[0]==priorCalls,"original executes after detour disable");
    for(void* target:targetsLocal)check(MH_RemoveHook(target)==MH_OK,"owned hook metadata removed");
    check(MH_Uninitialize()==MH_OK,"owned hook fixture cleaned up");

    resetQueue();for(unsigned i=0;i<Capacity+1;++i)enqueue(sample);
    count=0;while(queue.pop(sample))++count;
    check(count==Capacity&&dropped==1&&!recording,"queue overflow bounded and explicit");
    resetQueue();for(unsigned i=0;i<MaxRecords+10;++i) {enqueue(sample);check(queue.pop(sample)==(i<MaxRecords),"strict record limit");}
    check(reserved==MaxRecords&&committed==MaxRecords&&truncated&&!recording,"capture truncation explicit");
    resetQueue();
    logFile=CreateFileW(L"owned-admission-fixture.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(logFile!=INVALID_HANDLE_VALUE,"owned log opened");
    Sample acceptedSample;acceptedSample.completed=true;acceptedSample.scope=0;acceptedSample.queries=1;acceptedSample.queryKnown=true;
    acceptedSample.queryReturnMatches=true;acceptedSample.queryHandle=1;acceptedSample.selectionKnown=true;acceptedSample.selection={2,1};
    acceptedSample.selections=1;acceptedSample.requestKnown=true;acceptedSample.request.selection=acceptedSample.selection;
    acceptedSample.attemptReturned=true;acceptedSample.attemptByte=1;
    acceptedSample.frameworks=1;acceptedSample.frameworkReturned=true;acceptedSample.frameworkByte=1;enqueue(acceptedSample);
    Sample unknown;unknown.completed=true;unknown.attemptReturned=true;enqueue(unknown);stopping=true;writer(nullptr);
    CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    FILE* file=nullptr;check(!fopen_s(&file,"owned-admission-fixture.jsonl","rb"),"owned log readable");
    char text[8192]{};const auto size=fread(text,1,sizeof text-1,file);fclose(file);
    check(size>0&&strstr(text,"\"framework_accepted\":true")&&strstr(text,"\"query_position\":null")&&
        strstr(text,"\"stage\":\"before_query\"")&&strstr(text,"\"event\":\"stopped\"")&&!strstr(text,"0x")&&!strstr(text,"\"actor\":"),"bounded pointer-free true/null JSONL and shutdown");
    check(DeleteFileW(L"owned-admission-fixture.jsonl")!=0,"owned log removed");
    recording=true;writeFailed=false;
    check(!writeLine("{}\n")&&writeFailed&&!recording,"writer failure stops capture");
    // Reuse actual four-detour observations for parser conformance, including
    // the real production header/formatter. Only session accounting is reset.
    resetQueue();started=2;ended=2;ownerRefused=0;unreadable=0;contextOverflow=0;unlinkedFramework=0;
    calls[0]=2;calls[1]=2;calls[2]=2;calls[3]=1;
    for(auto& counter:requestGuards)counter=0;requestGuards[static_cast<unsigned>(RequestGuard::Matched)]=1;
    logFile=CreateFileW(L"admission-conformance.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(logFile!=INVALID_HANDLE_VALUE&&writeHeader(),"conformance header");
    Sample a=acceptedNative,m=missingNative;a.transaction=1;m.transaction=2;
    enqueue(a);enqueue(m);stopping=true;writer(nullptr);
    CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    puts("ADMISSION FIXTURES PASS: four native detours, exact forwarding/returns, query/selection/framework association, unknowns, scoped TLS, exceptions, parallel bounds, restoration and audit.");
}
