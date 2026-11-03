// Observe host-issued animation adjustment sweeps. Forward every call unchanged.
#include "../StateBridge/WindowsTarget.h"
#include "Clearance.h"
#include "MinHook.h"
#include <atomic>
#include <cwchar>
#include <intrin.h>
#include "Capture.h"

namespace {
using Query=float (__fastcall *)(void*,void*,void*,const char*);
Query original=nullptr;
constexpr auto Entry=ktclearance::Entry;
constexpr wchar_t CaptureName[]=L"clearance";
constexpr char Header[]=R"({"event":"started","probe":"clearance","revision":1,"motion_compiled":false,"record_limit":4096})" "\n";
std::atomic<unsigned> matched{0},unreadable{0},overflow{0};
constexpr unsigned Limit=4096;
struct Sample {
    ktclearance::Sweep sweep;
    long long tick=0;
    DWORD thread=0;
    float result=0;
    unsigned side=0;
    bool known=false,returned=false;
};
Sample samples[Limit];

template<class T> T field(const void* source,std::size_t offset) {
    T value;
    memcpy(&value,static_cast<const unsigned char*>(source)+offset,sizeof value);
    return value;
}

bool readSweep(void* world,void* params,void* lock,const char* tag,Sample& sample) {
    __try {
        if(!world||!params||lock||reinterpret_cast<std::uintptr_t>(tag)!=gameBase+ktclearance::Tag||
           field<void*>(reinterpret_cast<void*>(gameBase+ktclearance::WorldGlobal),0)!=world||
           field<std::uintptr_t>(world,0)!=gameBase+ktclearance::WorldVtable||
           field<unsigned>(params,0x18)!=0||field<unsigned>(params,0x98)!=0x10117||
           field<unsigned>(params,0xa8)!=0||field<unsigned>(params,0xac)!=3) return false;
        const auto box=field<const unsigned char*>(params,0x20);
        if(!box) return false;
        memcpy(sample.sweep.center,box+0x28,sizeof sample.sweep.center);
        memcpy(sample.sweep.halfExtent,box+0x34,sizeof sample.sweep.halfExtent);
        memcpy(sample.sweep.delta,static_cast<const unsigned char*>(params)+0x8c,sizeof sample.sweep.delta);
        return ktclearance::valid(sample.sweep,sample.side);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

float __fastcall observed(void* world,void* params,void* lock,const char* tag) {
    const auto address=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;
    Sample* sample=nullptr;
    float result=0;
    bool returned=false;
    __try {
        if(recording.load(std::memory_order_acquire)) {
            const auto side=ktclearance::direction(address-gameBase);
            if(side) {
                const unsigned index=matched.fetch_add(1,std::memory_order_relaxed);
                if(index<Limit) {
                    sample=&samples[index];
                    LARGE_INTEGER now;
                    QueryPerformanceCounter(&now);
                    sample->tick=now.QuadPart;
                    sample->thread=GetCurrentThreadId();
                    sample->side=side;
                    sample->known=readSweep(world,params,lock,tag,*sample);
                    if(!sample->known) ++unreadable;
                } else {
                    ++overflow;
                    recording=false;
                }
            }
        }
        result=original(world,params,lock,tag);
        returned=true;
    } __finally {
        if(sample) {sample->result=result;sample->returned=returned;}
        --inFlight;
    }
    return result;
}

bool pins() {
    __try {
        if(memcmp(reinterpret_cast<void*>(gameBase+ktclearance::Entry),ktclearance::EntryBytes,16)||
           field<std::uintptr_t>(reinterpret_cast<void*>(gameBase+ktclearance::WorldVtable),0x1c8)!=gameBase+ktclearance::Entry)
            return false;
        for(const auto rva:ktclearance::Returns)
            if(memcmp(reinterpret_cast<void*>(gameBase+rva-6),ktclearance::CallBytes,6)) return false;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool saveCapture() {
    const unsigned count=matched;
    char line[512];
    for(unsigned index=0;index<count&&index<Limit;++index) {
        const auto& sample=samples[index];
        const auto& sweep=sample.sweep;
        char center[96]="null",extent[96]="null",delta[96]="null",result[48]="null";
        if(sample.known) {
            sprintf_s(center,"[%.9g,%.9g,%.9g]",sweep.center[0],sweep.center[1],sweep.center[2]);
            sprintf_s(extent,"[%.9g,%.9g,%.9g]",sweep.halfExtent[0],sweep.halfExtent[1],sweep.halfExtent[2]);
            sprintf_s(delta,"[%.9g,%.9g,%.9g]",sweep.delta[0],sweep.delta[1],sweep.delta[2]);
        }
        if(sample.returned&&std::isfinite(sample.result)) sprintf_s(result,"%.9g",sample.result);
        sprintf_s(line,"{\"event\":\"sweep\",\"sequence\":%u,\"tick\":%lld,\"thread\":%lu,\"side\":%u,\"center\":%s,\"half_extent\":%s,\"delta\":%s,\"result\":%s,\"returned\":%s,\"used_by_adjustment\":%s}\n",
            index+1,sample.tick,sample.thread,sample.side,center,extent,delta,result,sample.returned?"true":"false",
            sample.known&&sample.returned&&ktclearance::usedByAdjustment(sweep,sample.side,sample.result)?"true":"false");
        if(!writeLine(line)) return false;
    }
    sprintf_s(line,"{\"event\":\"stopped\",\"matched\":%u,\"unreadable\":%u,\"overflow\":%u}\n",count,unreadable.load(),overflow.load());
    return writeLine(line)&&FlushFileBuffers(capture);
}
}

#include "Session.inl"
