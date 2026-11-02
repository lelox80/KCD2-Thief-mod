// Owned-process fixtures. Never loads or opens a game process.
#include "Probe.cpp"
#include <thread>
#include <vector>
#include <cstdlib>

void check(bool value, const char* message) {
    if (!value) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
std::atomic<unsigned> forwarded{0};
__declspec(noinline) bool __fastcall fixtureRequest(void* a, void* b) {
    ++forwarded;
    return a == b;
}
int main() {
    FILE* hashFixture=nullptr;
    check(fopen_s(&hashFixture,"probe-sha-fixture.bin","wb")==0,"create owned hash fixture");
    check(fwrite("abc",1,3,hashFixture)==3,"write owned hash fixture");fclose(hashFixture);
    char hash[65]{};
    check(sha256(L"probe-sha-fixture.bin",hash)&&!strcmp(hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"SHA-256 identity check");
    check(DeleteFileW(L"probe-sha-fixture.bin")!=0,"remove owned hash fixture");
    check(KtProbeStart(nullptr)==ERROR_MOD_NOT_FOUND,"fail closed when WHGame is absent");
    check(KtProbeStart(nullptr)==ERROR_ALREADY_INITIALIZED,"refuse duplicate initialization");
    check(KtProbeStop(nullptr)==ERROR_NOT_READY,"stop before active probe");
    unsigned char request[0x1b0]{};
    const unsigned long long flags=0x24;
    const float values[]={1.5f,2.f,3.f,4.f,5.f};
    memcpy(request,&flags,8);memcpy(request+0x38,values,8);memcpy(request+0x7c,values+2,12);
    auto fakeVtable=kControllerVtable;
    Sample observed{};
    check(readSample(&fakeVtable,request,observed)&&observed.flags==flags&&observed.speed==1.5f&&observed.z==5.f,"observed scalar layout on owned storage");
    check(!readSample(nullptr,request,observed),"invalid read refuses without crashing");
    Sample before{}; before.flags=0x24; before.speed=2; before.x=3;
    enqueue(before); Sample after{};
    check(pop(after) && !memcmp(&before,&after,sizeof before),"queue preserves sample");
    check(!pop(after),"queue empties");
    for(unsigned i=0;i<3000;++i) enqueue(before);
    unsigned fullCount=0;while(pop(after)) ++fullCount;
    check(fullCount==kCapacity-1&&dropped.load()==3000-fullCount,"bounded queue overflow");
    dropped=0;
    std::vector<std::thread> threads;
    for (unsigned n=0;n<4;++n) threads.emplace_back([] { for (unsigned i=0;i<500;++i) { Sample s{}; s.tid=GetCurrentThreadId(); enqueue(s); } });
    for (auto& t:threads) t.join();
    unsigned count=0; while(pop(after)) ++count;
    check(count+dropped.load()==2000,"parallel queue accounts for every sample or drop");
    check(MH_Initialize()==MH_OK,"MinHook initialization");
    check(MH_CreateHook(&fixtureRequest,&observedRequest,reinterpret_cast<void**>(&original))==MH_OK,"actual observer detour on owned fixture");
    check(MH_EnableHook(&fixtureRequest)==MH_OK,"owned fixture enable");
    RequestFn volatile call=&fixtureRequest;
    int a=1,b=2;
    check(call(&a,&a) && !call(&a,&b) && forwarded==2,"forwarded arguments and result");
    check(calls==2,"actual observation wrapper executed");
    check(MH_DisableHook(&fixtureRequest)==MH_OK,"owned fixture disable");
    check(call(&a,&a) && forwarded==3,"original after disable");
    check(MH_RemoveHook(&fixtureRequest)==MH_OK && MH_Uninitialize()==MH_OK,"fixture cleanup");
    puts("PROBE FIXTURES PASS: no-game refusal, bounded parallel queue, detour forwarding and restoration.");
}
