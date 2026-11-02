// Owned-process fixtures only. Never opens, loads or attaches to a game.
#include "Probe.cpp"
#include <cstdlib>
#include <thread>
#include <vector>

void check(bool ok,const char* message) {
    if(!ok) {fprintf(stderr,"FAIL: %s\n",message);exit(1);}
}
std::atomic<unsigned> forwarded{0};
void* seenActor=nullptr;ktledge::Selection* seenOutput=nullptr;bool seenAlternate=false;
__declspec(noinline) void __fastcall fixtureSelection(void* actor,ktledge::Selection* output,bool alternate) {
    ++forwarded;seenActor=actor;seenOutput=output;seenAlternate=alternate;
    output->actionType=alternate?2:0;output->handle=0x12345678;
}
void __fastcall sehSelection(void*,ktledge::Selection*,bool) {RaiseException(0xe04b7401,0,0,nullptr);}
bool catchHostSeh() {
    __try {observedSelection(nullptr,nullptr,false);return false;}
    __except(GetExceptionCode()==0xe04b7401?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return true;}
}
void __fastcall cppSelection(void*,ktledge::Selection*,bool) {throw 19;}
void drain() {Sample s;while(queue.pop(s)) {}}
int main() {
    check(KtProbeStart(nullptr)==ERROR_MOD_NOT_FOUND,"absent game fails closed");
    check(KtProbeStart(nullptr)==ERROR_ALREADY_INITIALIZED,"one initialization attempt");
    check(KtProbeStop(nullptr)==ERROR_NOT_READY,"stop before start");
    ktledge::Selection pair;check(pair.actionType==-1&&pair.handle==0xffffffffu,"no candidate sentinel");
    ktledge::Selection copy;
    check(readSelection(&pair,copy)&&copy.actionType==-1&&copy.handle==0xffffffffu,"8-byte pair read");
    check(!readSelection(nullptr,copy),"invalid output read refuses");
    std::uint32_t owner=0;check(!ownerId(nullptr,owner),"invalid actor refuses");
    alignas(16) unsigned char actor[0x188]{},entity[0x20]{},controller[0x20]{};
    auto setptr=[](unsigned char* bytes,std::size_t offset,std::uintptr_t value) {memcpy(bytes+offset,&value,sizeof value);};
    auto setid=[](unsigned char* bytes,std::size_t offset,std::uint32_t value) {memcpy(bytes+offset,&value,sizeof value);};
    gameBase=0;
    setptr(actor,0,ktretail::PlayerVtable);setptr(entity,0,ktretail::EntityVtable);
    setptr(controller,0,ktretail::ControllerVtable);setptr(controller,8,reinterpret_cast<std::uintptr_t>(actor));
    setptr(actor,0x180,reinterpret_cast<std::uintptr_t>(controller));
    setptr(actor,0x38,reinterpret_cast<std::uintptr_t>(entity));
    setid(actor,0x30,73);setid(entity,0xc,73);
    check(ownerId(actor,owner)&&owner==73,"full actor/controller/entity ownership chain");
    setid(entity,0xc,74);check(!ownerId(actor,owner),"entity identity mismatch refuses");setid(entity,0xc,73);
    recording=true;
    observeReturned(ktledge::ReturnRva+1,actor,&pair,false);
    check(matched==0&&reserved==0,"unproved caller excluded");
    observeReturned(ktledge::ReturnRva,actor,&pair,true);
    Sample sample;
    check(queue.pop(sample)&&sample.entity==73&&sample.readable&&sample.selection.actionType==-1&&sample.allowAlternate,"exact caller captures copied result and ID");
    observeReturned(ktledge::ReturnRva,actor,nullptr,false);
    check(queue.pop(sample)&&!sample.readable&&unreadable==1,"invalid post-read retains explicit null sample");
    observeReturned(ktledge::ReturnRva,nullptr,&pair,false);
    check(ownerRefused==1&&!queue.pop(sample),"invalid owner yields no sample");
    recording=false;
    observeReturned(ktledge::ReturnRva,actor,&pair,false);
    check(!queue.pop(sample),"stopped observer is passive");

    // Queue pressure must never wait or allocate, and the record budget is finite.
    reserved=0;dropped=0;recording=true;
    for(unsigned i=0;i<Capacity+1;++i) enqueue(sample);
    unsigned count=0;while(queue.pop(sample)) ++count;
    check(count==Capacity&&dropped==1&&!recording,"queue overflow disables capture");
    reserved=0;dropped=0;recording=true;
    for(unsigned i=0;i<MaxRecords+10;++i) {enqueue(sample);check(queue.pop(sample)==(i<MaxRecords),"strict sample budget");}
    check(reserved==MaxRecords&&!recording,"sample cap cannot be exceeded");
    reserved=0;dropped=0;recording=true;
    std::vector<std::thread> producers;
    for(unsigned n=0;n<4;++n) producers.emplace_back([] {for(unsigned i=0;i<400;++i) {Sample value;enqueue(value);}});
    for(auto& t:producers) t.join();
    count=0;while(queue.pop(sample)) ++count;
    check(count+dropped.load()==1600,"parallel producer sample accounting");recording=false;

    // Exercise real MinHook on an owned process and keep all argument/output bytes.
    check(MH_Initialize()==MH_OK,"MinHook initialized");
    check(MH_CreateHook(&fixtureSelection,&observedSelection,reinterpret_cast<void**>(&original))==MH_OK,"fixture hook created");
    check(MH_EnableHook(&fixtureSelection)==MH_OK,"fixture hook enabled");
    ktledge::SelectionFn volatile call=&fixtureSelection;
    call(actor,&pair,true);
    check(forwarded==1&&seenActor==actor&&seenOutput==&pair&&seenAlternate&&pair.actionType==2&&pair.handle==0x12345678&&inFlight==0,"same argument pointers, bool, host output and one original call");
    const auto hookedCalls=calls.load();
    check(MH_DisableHook(&fixtureSelection)==MH_OK,"fixture hook disabled");
    call(actor,&pair,false);
    check(forwarded==2&&calls==hookedCalls&&pair.actionType==0&&pair.handle==0x12345678,"original restored after disable");
    check(MH_RemoveHook(&fixtureSelection)==MH_OK&&MH_Uninitialize()==MH_OK,"owned fixture cleanup");
    original=&sehSelection;
    check(catchHostSeh()&&inFlight==0,"original SEH propagates and in-flight counter cleans up");
    original=&cppSelection;bool caught=false;
    try {observedSelection(actor,&pair,false);} catch(int value) {caught=value==19;}
    check(caught&&inFlight==0,"original C++ exception propagates and in-flight counter cleans up");

    // Writer-only file operations produce finite, pointer-free JSONL.
    logFile=CreateFileW(L"owned-ledge-fixture.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(logFile!=INVALID_HANDLE_VALUE,"owned writer fixture file");
    reserved=0;writeFailed=false;stopping=false;recording=true;
    Sample valid;valid.entity=73;valid.readable=true;valid.selection=pair;enqueue(valid);
    Sample invalid;invalid.entity=73;enqueue(invalid);stopping=true;
    check(writer(nullptr)==0,"writer drains after stop");CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    FILE* file=nullptr;check(fopen_s(&file,"owned-ledge-fixture.jsonl","rb")==0,"read owned log");
    char text[2048]{};const auto length=fread(text,1,sizeof text-1,file);fclose(file);
    check(length>0&&strstr(text,"\"action_type\":0")&&strstr(text,"\"action_type\":null")&&strstr(text,"\"event\":\"stopped\"")&&!strstr(text,"actor")&&!strstr(text,"controller"),"JSONL candidate/null/stop with no raw ownership pointers");
    check(DeleteFileW(L"owned-ledge-fixture.jsonl")!=0,"remove owned writer fixture");
    recording=true;writeFailed=false;
    check(!writeLine("{}\n")&&writeFailed&&!recording,"writer failure disables recording");
    puts("LEDGE PROBE FIXTURES PASS: no-game refusal, ownership/caller/output checks, bounded parallel queue, forwarding, SEH/C++ cleanup, restoration and writer audit.");
}
