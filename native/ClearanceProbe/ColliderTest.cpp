#include "Collider.cpp"

namespace {
void* forwardedController=nullptr;
void* forwardedRequest=nullptr;
bool __fastcall host(void* controller,void* request) {
    forwardedController=controller;
    forwardedRequest=request;
    return true;
}
#ifdef KT_CAPSULE_CONTROL
volatile LONG fixtureLock=2;
float __fastcall fakeQuery(void*,ktcapsule::Packet* packet,void* lock,const char*) {
    if(lock||packet->type!=6||packet->primitive->center.z!=9.25f||packet->delta.z!=-0.75f||
       packet->skipCount!=1||*packet->skip!=&forwardedController||packet->intersectionOverride||
       packet->lockState!=&packet->lockActive) return -99;
    packet->lockActive=2;
    packet->lockState=const_cast<LONG*>(&fixtureLock);
    return 0.5f;
}
#endif
}

int main() {
    Dimensions dimensions;
    dimensions.size[0]=0.3f;
    dimensions.size[2]=0.4f;
    dimensions.center=1.05f;
    dimensions.capsule=1;
    if(!valid(dimensions)) return 1;
    dimensions.capsule=2;
    if(valid(dimensions)) return 2;
    dimensions.capsule=1;
    dimensions.size[0]=0;
    if(valid(dimensions)) return 3;
    original=host;
    recording=true;
    int controller=0,request=0;
    if(!observed(&controller,&request)||forwardedController!=&controller||
       forwardedRequest!=&request||captured||inFlight) return 4;
    Sample sample;
    readDimensions(nullptr,sample);
    if(strcmp(sample.result,"owner_refused")) return 5;
#ifdef KT_CAPSULE_CONTROL
    const ktcapsule::Shape shape{{5,6,9.25f},{0,0,1},0.35f,0.4f};
    if(ktcapsule::query(fakeQuery,nullptr,nullptr,&forwardedController,shape,{0,0,-0.75f})!=0.5f||fixtureLock!=0) return 6;
#endif
    puts("Player collider fixture passed");
}
