// Read the canonical player's live living-entity dimensions on its normal
// controller callback. No queries are injected into the world.
#include "../StateBridge/WindowsTarget.h"
#include "MinHook.h"
#include <atomic>
#include <cstddef>
#include <cmath>
#include <cwchar>
#include <intrin.h>
#include "Capture.h"
#if defined(KT_GROUND_CAPTURE)
#include "../LedgeProbe/VaultTrial.h"
#elif defined(KT_CAPSULE_CONTROL)
#include "CapsuleControl.h"
#endif

namespace {
constexpr auto Entry=ktretail::Request;
constexpr std::uintptr_t PhysicalGetter=0x8a49fc,LivingVtable=0x3a82570,GetParams=0x3d5384;
#ifdef KT_CAPSULE_CONTROL
constexpr wchar_t CaptureName[]=L"capsule-control";
#ifdef KT_TALL_ROUTE
constexpr auto Corridor=ktcapsule::TallWall;
constexpr char Header[]=R"({"event":"started","probe":"capsule-control","revision":3,"motion_compiled":false,"query_limit":10,"route":"tall_wall"})" "\n";
#else
constexpr auto Corridor=ktcapsule::RaisedPavement;
constexpr char Header[]=R"({"event":"started","probe":"capsule-control","revision":2,"motion_compiled":false,"query_limit":10})" "\n";
#endif
std::atomic<bool> controlClaimed{false};
ktcapsule::Control control;
constexpr unsigned SampleStride=32;
#else
constexpr wchar_t CaptureName[]=L"collider";
#ifdef KT_GROUND_CAPTURE
constexpr char Header[]=R"({"event":"started","probe":"player-collider","revision":2,"motion_compiled":false,"record_limit":128})" "\n";
#else
constexpr char Header[]=R"({"event":"started","probe":"player-collider","revision":1,"motion_compiled":false,"record_limit":128})" "\n";
#endif
constexpr unsigned SampleStride=8;
#endif
using Request=bool (__fastcall *)(void*,void*);
using GetPhysical=void* (__fastcall *)(void*);
using GetDimensions=int (__fastcall *)(void*,void*);
Request original=nullptr;

struct Dimensions {
    std::uint32_t type=1;
    float pivot=0,eye=0,size[3]{},center=0,headRadius=0,headCenter=0;
    unsigned char unused[0x10]{};
    std::uint32_t capsule=0;
    float groundContact=0;
};
static_assert(sizeof(Dimensions)==0x3c&&offsetof(Dimensions,capsule)==0x34);
struct Sample {
    Dimensions dimensions;
    long long tick=0;
    std::uint32_t entity=0;
    const char* result="owner_refused";
#ifdef KT_GROUND_CAPTURE
    ktvault::LivingStatus ground;
    ktcapsule::PosePacket pose;
    bool groundRead=false,poseRead=false;
#endif
};
constexpr unsigned Limit=128;
Sample samples[Limit];
std::atomic<unsigned> eligible{0},captured{0};

bool valid(const Dimensions& dimensions) {
    if(dimensions.type!=1||dimensions.capsule>1||!std::isfinite(dimensions.pivot)||
       !std::isfinite(dimensions.center)||dimensions.size[0]<=0) return false;
    for(float size:dimensions.size)
        if(!std::isfinite(size)||size<0||size>10) return false;
    return std::abs(dimensions.pivot)<10&&std::abs(dimensions.center)<10;
}

void readDimensions(void* controller,Sample& sample) {
    __try {
        std::uint64_t actor=0;
        if(!ktwin::owner(gameBase,controller,sample.entity,actor)) return;
        auto entity=*reinterpret_cast<void**>(actor+0x38);
        auto physical=reinterpret_cast<GetPhysical>(gameBase+PhysicalGetter)(entity);
        sample.result="living_entity_refused";
        if(!physical||*reinterpret_cast<std::uintptr_t*>(physical)!=gameBase+LivingVtable) return;
        sample.result="dimensions_refused";
        if(reinterpret_cast<GetDimensions>(gameBase+GetParams)(physical,&sample.dimensions)==1&&
           valid(sample.dimensions)) sample.result="observed";
#ifdef KT_GROUND_CAPTURE
        const auto status=reinterpret_cast<ktcapsule::ReadStatus>(gameBase+ktcapsule::Status);
        sample.groundRead=status(physical,&sample.ground)==1&&std::isfinite(sample.ground.groundHeight);
        sample.poseRead=status(physical,&sample.pose)==1&&std::isfinite(sample.pose.position.x)&&
            std::isfinite(sample.pose.position.y)&&std::isfinite(sample.pose.position.z);
#endif
#ifdef KT_CAPSULE_CONTROL
        const auto& d=sample.dimensions;
        if(!strcmp(sample.result,"observed")&&d.capsule==1&&std::abs(d.pivot)<0.01f&&
           std::abs(d.center-1.05f)<0.01f&&std::abs(d.size[0]-0.35f)<0.01f&&
           std::abs(d.size[2]-0.4f)<0.01f&&!controlClaimed.exchange(true)) {
            recording=false;
            sample.result=ktcapsule::run(gameBase,physical,d.center,d.size[0],d.size[2],control,false,Corridor)?
                "controls_observed":"controls_refused";
        }
#endif
    } __except(EXCEPTION_EXECUTE_HANDLER) {sample.result="read_fault";}
}

bool __fastcall observed(void* controller,void* request) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;
    bool result=false;
    __try {
        if(recording&&caller==gameBase+ktretail::PlayerReturn) {
            const auto frame=eligible.fetch_add(1);
            const auto index=frame/SampleStride;
            if(frame%SampleStride==0&&index<Limit) {
                auto& sample=samples[index];
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                sample.tick=now.QuadPart;
                readDimensions(controller,sample);
                ++captured;
            }
            if(index>=Limit) recording=false;
        }
        result=original(controller,request);
    } __finally {--inFlight;}
    return result;
}

bool pins() {
    __try {
#ifdef KT_GROUND_CAPTURE
        if(*reinterpret_cast<std::uintptr_t*>(gameBase+LivingVtable+0x30)!=gameBase+ktcapsule::Status||
           !ktretail::bytesMatch(gameBase,{ktcapsule::Status,"488bc448895808555657415441554156"})) return false;
#endif
#ifdef KT_CAPSULE_CONTROL
        if(*reinterpret_cast<std::uintptr_t*>(gameBase+LivingVtable+0x30)!=gameBase+ktcapsule::Status||
           !ktcapsule::pins(gameBase)) return false;
#endif
        return *reinterpret_cast<std::uintptr_t*>(gameBase+LivingVtable+0x28)==gameBase+GetParams&&
            ktretail::bytesMatch(gameBase,{GetParams,"488bc44889580848896810574883ec40"})&&
            ktretail::bytesMatch(gameBase,{PhysicalGetter,"40534883ec20ba01000000488bd9e8b5"});
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

bool saveCapture() {
    char line[512];
    const auto count=captured.load();
    for(unsigned i=0;i<count;++i) {
        const auto& sample=samples[i];
        const auto& d=sample.dimensions;
        char fields[240]="null";
        if(!strcmp(sample.result,"observed")||!strcmp(sample.result,"controls_observed"))
            sprintf_s(fields,"{\"pivot\":%.9g,\"center\":%.9g,\"size\":[%.9g,%.9g,%.9g],\"capsule\":%u}",
                d.pivot,d.center,d.size[0],d.size[1],d.size[2],d.capsule);
        sprintf_s(line,"{\"event\":\"dimensions\",\"sequence\":%u,\"tick\":%lld,\"entity\":%u,\"result\":\"%s\",\"dimensions\":%s",
            i+1,sample.tick,sample.entity,sample.result,fields);
#ifdef KT_GROUND_CAPTURE
        char extra[160];
        if(sample.groundRead) {
            sprintf_s(extra,",\"flying\":%d,\"ground_height\":%.9g",sample.ground.flying,sample.ground.groundHeight);
            strcat_s(line,extra);
        }
        if(sample.poseRead) {
            const auto p=sample.pose.position;
            sprintf_s(extra,",\"position\":[%.9g,%.9g,%.9g]",p.x,p.y,p.z);
            strcat_s(line,extra);
        }
#endif
        strcat_s(line,"}\n");
        if(!writeLine(line)) return false;
    }
#ifdef KT_CAPSULE_CONTROL
    if(control.complete) {
        const auto& s=control.shape;
        sprintf_s(line,"{\"event\":\"controls\",\"center\":[%.9g,%.9g,%.9g],\"radius\":%.9g,\"half_span\":%.9g,\"delta_z\":%.9g,\"at_pose\":%.9g,\"below_pose\":%.9g,\"down_sweep\":%.9g}\n",
            s.center.x,s.center.y,s.center.z,s.radius,s.halfSpan,ktcapsule::GroundDelta,control.atPose,control.belowPose,control.downSweep);
        if(!writeLine(line)) return false;
    }
    if(control.routeComplete) {
        const auto& o=control.overlaps;
        const auto& s=control.sweeps;
        sprintf_s(line,"{\"event\":\"wall_route\",\"overlaps\":[%.9g,%.9g,%.9g,%.9g],\"sweeps\":[%.9g,%.9g,%.9g]}\n",
            o[0],o[1],o[2],o[3],s[0],s[1],s[2]);
        if(!writeLine(line)) return false;
    }
#endif
    sprintf_s(line,"{\"event\":\"stopped\",\"captured\":%u}\n",count);
    return writeLine(line)&&FlushFileBuffers(capture);
}
}

#include "Session.inl"
