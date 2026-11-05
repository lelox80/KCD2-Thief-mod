#pragma once

#include "Capsule.h"

namespace ktcapsule {
constexpr std::uintptr_t WorldGlobal=0x492d830,WorldVtable=0x407fc80,Intersection=0x8115bc;
constexpr std::uintptr_t Status=0x8a58a0,Tag=0x3a56588;
constexpr float GroundDelta=-0.75f;
using ReadStatus=int (__fastcall *)(void*,void*);
using Query=float (__fastcall *)(void*,Packet*,void*,const char*);
struct PosePacket {
    std::int32_t type=1,partId=-1,partIndex=-1;
    std::uint32_t flags=0,flagsOr=0,flagsAnd=0;
    Vector position;
    unsigned char storage[0x100-0x24]{};
};
static_assert(offsetof(PosePacket,position)==0x18&&sizeof(PosePacket)==0x100);

struct Control {
    Shape shape;
    float atPose=0,belowPose=0,downSweep=0;
    bool complete=false;
    bool routeComplete=false;
    bool sectionRefused=false;
    bool landingRefused=false;
    float overlaps[4]{},sweeps[3]{};
};

struct Corridor {
    Vector approach,landing;
    float crossingZ;
};
struct LedgeSection {
    Vector lip,facing;
    bool crestOnly=false;
};
constexpr Corridor RaisedPavement{{2897.048096f,766.461975f,61.337589f},
    {2897.150635f,767.440430f,62.340530f},62.50f};
constexpr Corridor TallWall{{2896.298584f,794.456909f,64.318535f},
    {2894.961182f,794.222778f,65.958130f},66.11f};

inline bool pins(std::uintptr_t base) {
    return *reinterpret_cast<std::uintptr_t*>(base+WorldVtable+0x1c8)==base+Intersection&&
        ktretail::bytesMatch(base,{Intersection,"488bc448895808488968104889701848"})&&
        ktretail::bytesMatch(base,{Status,"488bc448895808555657415441554156"})&&
        !memcmp(reinterpret_cast<const char*>(base+Tag),"PrimitiveWorldIntersection",sizeof "PrimitiveWorldIntersection");
}

inline float query(Query host,void* world,const char* tag,void* physical,Shape shape,Vector delta) {
    Packet packet;
    packet.primitive=&shape;
    packet.delta=delta;
    packet.skipCount=1;
    packet.skip=&physical;
    float result=0;
    __try {result=host(world,&packet,nullptr,tag);}
    __finally {
        // Same release as the host action and capsule overlap callers.
        if(packet.lockActive)
            InterlockedExchangeAdd(static_cast<volatile LONG*>(packet.lockState),-packet.lockActive);
    }
    return result;
}

inline bool groundControlsPass(const Control& control) {
    return control.complete&&control.atPose==0&&control.belowPose>0&&
        control.downSweep>0&&control.downSweep<-GroundDelta;
}

inline bool ledgeSectionClear(Query host,void* world,const char* tag,void* physical,const LedgeSection& section) {
    const auto lip=section.lip,f=section.facing;
    float faceDepth=0;
    // Survey across a body-width strip at the projected grab point. Each lane
    // needs a face close to the guide and a supported top just behind it.
    constexpr float offsets[]={0,-0.22f,0.22f};
    for(float side:offsets) {
        const float x=lip.x+side*f.y,y=lip.y-side*f.x;
        Shape sample{{x-f.x*0.45f,y-f.y*0.45f,lip.z-0.22f},{0,0,1},
                     section.crestOnly?0.018f:0.04f,section.crestOnly?0.01f:0.02f};
        const float reach=section.crestOnly?0.45f:0.62f;
        const auto face=query(host,world,tag,physical,sample,{f.x*reach,f.y*reach,0});
        if(!(face>(section.crestOnly?0.28f:0.23f)&&face<(section.crestOnly?0.445f:0.48f))) return false;
        if(side==0) faceDepth=face;
        else if(std::abs(face-faceDepth)>0.10f) return false;
        const float inset=section.crestOnly?0.0f:0.13f;
        sample.center={x+f.x*inset,y+f.y*inset,lip.z+0.23f};
        const auto top=query(host,world,tag,physical,sample,{0,0,-0.32f});
        if(!(top>0.10f&&top<0.24f)) return false;
    }
    return true;
}

// Bounded route between the archived v21 approach and center landing ray.
// The raised crossing is a diagnostic path. It is independent of animation.
inline void wallRoute(Query host,void* world,const char* tag,void* physical,float centerOffset,Control& output,
                      Vector approach,const Corridor& corridor) {
    const auto landing=corridor.landing;
    const Vector points[]={approach,{approach.x,approach.y,corridor.crossingZ},
        {landing.x,landing.y,corridor.crossingZ},landing};
    Shape shape=output.shape;
    for(unsigned i=0;i<4;++i) {
        const auto& point=points[i];
        shape.center={point.x,point.y,point.z+centerOffset};
        output.overlaps[i]=query(host,world,tag,physical,shape,{});
        if(!std::isfinite(output.overlaps[i])) return;
        if(i<3) {
            const auto& next=points[i+1];
            output.sweeps[i]=query(host,world,tag,physical,shape,
                {next.x-point.x,next.y-point.y,next.z-point.z});
            if(!std::isfinite(output.sweeps[i])) return;
        }
    }
    output.routeComplete=true;
}

// Same source and shape for three controls. Use the living-player position
// getter and the shipped PWI entry, with only the physical owner skipped.
inline bool run(std::uintptr_t base,void* physical,float center,float radius,float halfSpan,Control& output,
                bool fromLiveApproach=false,const Corridor& corridor=RaisedPavement,bool requireLandingSupport=false,
                const LedgeSection* section=nullptr,bool airborne=false,const Vector* adjustedTarget=nullptr) {
    const auto world=*reinterpret_cast<void**>(base+WorldGlobal);
    if(!world||*reinterpret_cast<std::uintptr_t*>(world)!=base+WorldVtable||
       *reinterpret_cast<void**>(static_cast<unsigned char*>(physical)+0x68)!=world) return false;
    PosePacket pose;
    if(reinterpret_cast<ReadStatus>(base+Status)(physical,&pose)!=1) return false;
    const auto p=pose.position;
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
       std::abs(p.x)>100000||std::abs(p.y)>100000||std::abs(p.z)>10000) return false;
    output.shape={{p.x,p.y,p.z+center},{0,0,1},radius,halfSpan};
    const auto host=reinterpret_cast<Query>(base+Intersection);
    const auto tag=reinterpret_cast<const char*>(base+Tag);
    output.atPose=query(host,world,tag,physical,output.shape,{});
    output.complete=std::isfinite(output.atPose);
    if(!airborne) {
        auto lowered=output.shape;
        lowered.center.z+=GroundDelta;
        output.belowPose=query(host,world,tag,physical,lowered,{});
        output.downSweep=query(host,world,tag,physical,output.shape,{0,0,GroundDelta});
        output.complete=output.complete&&std::isfinite(output.belowPose)&&std::isfinite(output.downSweep);
    }
    const auto anchor=corridor.approach;
    if((airborne?(output.complete&&output.atPose==0):groundControlsPass(output))&&std::abs(p.x-anchor.x)<8&&
       std::abs(p.y-anchor.y)<8&&std::abs(p.z-anchor.z)<2) {
        if(section&&!ledgeSectionClear(host,world,tag,physical,*section)) {
            output.sectionRefused=true;
            return true;
        }
        wallRoute(host,world,tag,physical,center,output,fromLiveApproach?p:anchor,corridor);
        if(output.routeComplete&&adjustedTarget) {
            auto shape=output.shape;
            const auto target=*adjustedTarget;
            shape.center={target.x,target.y,corridor.crossingZ+center};
            const auto descent=query(host,world,tag,physical,shape,{0,0,target.z-corridor.crossingZ});
            shape.center.z=target.z+center;
            output.routeComplete=descent==0&&query(host,world,tag,physical,shape,{})==0;
        }
        if(output.routeComplete&&requireLandingSupport) {
            auto landing=output.shape;
            landing.center={corridor.landing.x,corridor.landing.y,corridor.landing.z+center};
            // The bottom of the standing capsule sits about 0.30 m above
            // the physical origin. Confirm support near the authored top.
            const auto expected=center-radius-halfSpan;
            const auto hit=query(host,world,tag,physical,landing,{0,0,-(expected+0.09f)});
            output.routeComplete=expected>0.09f&&hit>expected-0.07f&&hit<expected+0.07f;
            output.landingRefused=!output.routeComplete;
        }
    }
    return output.complete;
}
}
