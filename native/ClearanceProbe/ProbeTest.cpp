#include "Probe.cpp"
#include <cstdlib>
#include <limits>

namespace {
void check(bool pass,const char* message) {
    if(!pass) {fprintf(stderr,"%s\n",message);std::exit(1);}
}
void* expected[3]{};
const char* expectedTag=nullptr;
unsigned forwarded=0;
float __fastcall host(void* world,void* params,void* lock,const char* tag) {
    check(world==expected[0]&&params==expected[1]&&lock==expected[2]&&tag==expectedTag,"argument forwarding");
    ++forwarded;
    return -3.25f;
}
template<class T> void set(unsigned char* buffer,std::size_t offset,T value) {
    memcpy(buffer+offset,&value,sizeof value);
}
}

int main() {
    void* world=nullptr;
    gameBase=reinterpret_cast<std::uintptr_t>(&world)-ktclearance::WorldGlobal;
    std::uintptr_t vtable=gameBase+ktclearance::WorldVtable;
    world=&vtable;
    const auto tag=reinterpret_cast<const char*>(gameBase+ktclearance::Tag);
    unsigned char box[0x40]{},params[0xe8]{};
    set(params,0x20,box+0);
    set(params,0x8c,1.0f);
    set(params,0x98,0x10117u);
    set(params,0xac,3u);
    set(box,0x28,100.0f);
    set(box,0x34,0.25f);
    Sample sample;
    sample.side=1;
    check(readSweep(world,params,nullptr,tag,sample),"read pinned sweep");
    check(sample.sweep.center[0]==100&&sample.sweep.halfExtent[0]==0.25f,"box field layout");
    check(ktclearance::usedByAdjustment(sample.sweep,1,0.5f),"in-range result");
    const float unusedResults[]={0.0f,-1.0f,1.0f,2.0f,std::numeric_limits<float>::quiet_NaN()};
    for(float result:unusedResults)
        check(!ktclearance::usedByAdjustment(sample.sweep,1,result),"outside caller result rule");
    set(params,0x98,0u);
    check(!readSweep(world,params,nullptr,tag,sample),"unexpected mask");
    check(!readSweep(world,reinterpret_cast<void*>(1),nullptr,tag,sample),"unreadable packet");
    check(!readSweep(world,params,nullptr,nullptr,sample),"wrong tag");
    sample.sweep.delta[0]=-1;
    check(!ktclearance::valid(sample.sweep,1)&&ktclearance::valid(sample.sweep,2),"negative X");
    sample.sweep.delta[0]=0;sample.sweep.delta[1]=1;
    check(ktclearance::valid(sample.sweep,3),"positive Y");
    sample.sweep.delta[1]=0;
    check(ktclearance::valid(sample.sweep,3)&&!ktclearance::usedByAdjustment(sample.sweep,3,0),"zero sweep is observable");
    sample.sweep.delta[1]=-1;
    check(ktclearance::valid(sample.sweep,4),"negative Y");
    sample.sweep.delta[2]=1;
    check(!ktclearance::valid(sample.sweep,4),"off axis");
    expected[0]=world;expected[1]=params;expected[2]=box;expectedTag=tag;
    original=host;
    recording=true;
    check(observed(world,params,box,tag)==-3.25f&&forwarded==1&&matched==0&&inFlight==0,
        "unrelated caller forwards original result exactly");
    puts("Clearance probe fixture passed");
}
