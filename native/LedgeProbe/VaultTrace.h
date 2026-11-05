#pragma once

#include "VaultTrial.h"

namespace kttrace {
constexpr std::uintptr_t GrabVtable=0x3e73ef8,GrabCreate=0xf45298,GrabCreateReturn=0xf4380d,GrabExit=0x702b7c;
constexpr char CreateBytes[]="4c8bdc49895b084989731049897b184d";
constexpr char ExitBytes[]="48895c2410574881eca0000000488b05";
constexpr std::uintptr_t LivingAction=0x845b90,AnimatedMoveReturn=0x8a39b1;
constexpr char ActionBytes[]="488bc448895810488970185557415441";
constexpr float CrestCrossSpeed=4.0f;
constexpr float MinCrestApex=0.23f;
using CreateFn=void* (__fastcall *)(void*,void*,const void*,std::uint32_t,float,float,float,
                                    std::uint8_t,std::uint8_t,std::uint8_t);
using ExitFn=void (__fastcall *)(void*);
using LivingActionFn=int (__fastcall *)(void*,const void*,int);

struct MovePacket {
    std::int32_t type=1;
    ktcapsule::Vector velocity{};
    std::int32_t jump=2; // Native additive velocity mode.
    float dt=0;
};
static_assert(sizeof(MovePacket)==0x18&&offsetof(MovePacket,jump)==0x10);

struct Dynamics {
    std::int32_t type=4;
    unsigned char inertia[0x10]{};
    ktcapsule::Vector gravity{};
    float nodSpeed=0;
    std::int32_t swimming=-1;
    unsigned char fields[0x34]{};
    std::int32_t active=0;
    unsigned char tail[0x100-0x60]{};
};
static_assert(offsetof(Dynamics,gravity)==0x14&&offsetof(Dynamics,active)==0x5c);

struct TargetPacket {
    float rotation[4];
    ktcapsule::Vector position;
};
static_assert(sizeof(TargetPacket)==0x1c&&offsetof(TargetPacket,position)==0x10);

inline bool crestCorrection(ktcapsule::Vector crest,ktcapsule::Vector facing,ktcapsule::Vector& target) {
    // Calibrated from the matched target and physical Exit sample on this wall.
    constexpr ktcapsule::Vector measured{2894.05469f,759.333496f,63.2343559f};
    constexpr ktcapsule::Vector forward{-0.025878f,-0.999665f,0};
    const float dx=crest.x-measured.x,dy=crest.y-measured.y;
    if(!(dx*dx+dy*dy<=0.55f*0.55f&&std::abs(crest.z-measured.z)<=0.08f&&
         facing.x*forward.x+facing.y*forward.y>=0.97f)) return false;
    target={crest.x-0.24f*facing.x,crest.y-0.24f*facing.y,crest.z+0.03f};
    return true;
}

inline bool matchesCrest(ktcapsule::Vector target,ktcapsule::Vector crest) {
    return std::abs(target.x-crest.x)<0.025f&&std::abs(target.y-crest.y)<0.025f&&
        std::abs(target.z-crest.z)<0.025f;
}

struct Record {
    std::uint64_t transaction=0,epoch=0,deadline=0;
    std::uintptr_t actor=0,grab=0;
    std::uint32_t entity=0,handle=0;
    ktcapsule::Vector crest,target;
    ktcapsule::Vector adjusted,stockTarget,facing;
    bool hasAdjustment=false,targetAdjusted=false;
};

struct Support {
    ktcapsule::Vector position;
    float groundHeight=0;
    int flying=-1;
    bool positionKnown=false;
};

struct CrestProfile {
    float rise=0,depth=0;
    unsigned samples=0;

    // The known wall produced a candidate collision 28 cm above the guide.
    float barrier() const {return rise>0.282f?rise:0.282f;}
    float pushHeight() const {return barrier()+0.085f;}
    float apex() const {return barrier()+0.19f;}
    float headroom() const {return apex()+0.045f;}
};

struct Event {
    enum Kind {None, Target, Release, PostRelease, PhysicalLift, PhysicalPush} kind=None;
    std::uint64_t transaction=0;
    std::uint32_t entity=0,handle=0;
    long long tick=0;
    ktcapsule::Vector crest,target;
    Support before,after;
    bool exitReturned=false;
    ktcapsule::Vector stockTarget;
    bool targetAdjusted=false;
    unsigned sequence=0;
    std::uint64_t elapsedMs=0;
    ktcapsule::Vector addedVelocity;
    float forwardOfCrest=0;
    int liftResult=0;
    const char* liftState=nullptr;
    ktcapsule::Vector releaseMotion,gravity;
    bool liftInputsKnown=false;
    CrestProfile profile;
};

struct ReleaseWatch {
    static constexpr std::uint64_t DurationMs=950,IntervalMs=16;
    static constexpr std::uint64_t LiftWindowMs=80,LiftRetryMs=8;
    static constexpr std::uint64_t PushWindowMs=320;
    static constexpr unsigned Limit=64;
    Record record;
    std::uint64_t start=0,next=0,nextLift=0;
    unsigned count=0;
    void* liftPhysical=nullptr;
    void* pushPhysical=nullptr;
    std::uint64_t pushDeadline=0;
    float pushDt=0;
    CrestProfile pushProfile;

    void begin(const Record& source,std::uint64_t now,void* physical=nullptr) {
        *this={source,now,now,now,0,physical};
    }

    bool expired(std::uint64_t now) const {
        return now<start||now-start>DurationMs||count>=Limit;
    }

    bool claim(std::uint64_t now) {
        if(expired(now)||now<next) return false;
        next=now+IntervalMs;
        ++count;
        return true;
    }

    bool canLift(void* physical,std::uint64_t now) const {
        return physical&&liftPhysical==physical&&now>=nextLift&&now-start<=LiftWindowMs;
    }

    void resolveLift(std::uint64_t now,bool retry) {
        if(retry) nextLift=now+LiftRetryMs;
        else liftPhysical=nullptr;
    }

    bool takeLiftTimeout(std::uint64_t now) {
        if(!liftPhysical||now<start||now-start<=LiftWindowMs) return false;
        liftPhysical=nullptr;
        return true;
    }

    void armPush(void* physical,std::uint64_t now,float dt,const CrestProfile& profile) {
        pushPhysical=physical;
        pushDeadline=now+PushWindowMs;
        pushDt=dt;
        pushProfile=profile;
    }

    bool canPush(void* physical,std::uint64_t now) const {
        return physical&&physical==pushPhysical&&now>=start&&now<=pushDeadline;
    }
};

inline bool finite(ktcapsule::Vector point) {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
}

constexpr char WaitingForGravity[]="waiting_for_gravity";

inline bool retryReleaseLift(const char* state) {
    return state==WaitingForGravity||!strcmp(state,"horizontal_velocity")||!strcmp(state,"vertical_velocity");
}

inline bool gravityInRange(ktcapsule::Vector gravity) {
    return std::abs(gravity.x)<0.05f&&std::abs(gravity.y)<0.05f&&gravity.z<=-8.0f&&gravity.z>=-14.0f;
}

inline bool horizontalVelocityInRange(ktcapsule::Vector velocity) {
    return velocity.x*velocity.x+velocity.y*velocity.y<=0.85f*0.85f;
}

inline const char* releaseVelocityRefusal(ktcapsule::Vector crest,ktcapsule::Vector position,
                                           ktcapsule::Vector velocity,ktcapsule::Vector gravity,float& upward,
                                           float apex=MinCrestApex) {
    upward=0;
    if(!finite(crest)||!finite(position)||!finite(velocity)||!finite(gravity)) return "nonfinite_kinematics";
    const float dx=position.x-crest.x,dy=position.y-crest.y,deficit=crest.z-position.z;
    if(dx*dx+dy*dy>0.05f*0.05f||deficit<=0||deficit>0.06f) return "crest_position";
    if(gravity.x==0&&gravity.y==0&&gravity.z==0) return WaitingForGravity;
    // Retail BasicActor sets 13 m/s². Use the live value in the lift calculation.
    if(!gravityInRange(gravity)) return "gravity";
    // The first post-release physics step can drift at about 0.75 m/s with no input.
    if(!horizontalVelocityInRange(velocity)) return "horizontal_velocity";
    const float delta=std::sqrt(-2.0f*gravity.z*(deficit+apex))-velocity.z;
    if(delta<=0||delta>5.4f) return "vertical_velocity";
    upward=delta;
    return nullptr;
}

enum class Push {Wait,Ready,Refused};

inline Push crestPushGate(ktcapsule::Vector crest,ktcapsule::Vector position,
                         ktcapsule::Vector velocity,ktcapsule::Vector gravity,float pushHeight,float headroom) {
    if(!finite(crest)||!finite(position)||!finite(velocity)||!finite(gravity)) return Push::Refused;
    const float dx=position.x-crest.x,dy=position.y-crest.y,height=position.z-crest.z;
    if(dx*dx+dy*dy>0.06f*0.06f||height<-0.06f||height>=headroom||
       !gravityInRange(gravity)||!horizontalVelocityInRange(velocity)||velocity.z<=0)
        return Push::Refused;
    return height>=pushHeight?Push::Ready:Push::Wait;
}

inline bool addCrestForward(ktcapsule::Vector facing,ktcapsule::Vector velocity,ktcapsule::Vector& delta) {
    const float length=facing.x*facing.x+facing.y*facing.y;
    if(!finite(facing)||std::abs(length-1.0f)>0.02f||std::abs(facing.z)>0.01f) return false;
    const float scale=1.0f/std::sqrt(length);
    const float x=facing.x*scale,y=facing.y*scale;
    // Match the crossing speed along the checked approach axis and keep lateral velocity.
    const float forward=CrestCrossSpeed-(velocity.x*x+velocity.y*y);
    delta.x=forward*x;
    delta.y=forward*y;
    return true;
}

inline bool releaseSweep(ktcapsule::Query host,void* world,const char* tag,void* physical,
                         const ktvault::Dimensions& dims,ktcapsule::Vector position,float rise) {
    ktcapsule::Shape shape{{position.x,position.y,position.z+dims.center},{0,0,1},dims.size[0],dims.size[2]};
    if(ktcapsule::query(host,world,tag,physical,shape,{})!=0||
       ktcapsule::query(host,world,tag,physical,shape,{0,0,rise})!=0) return false;
    shape.center.z+=rise;
    return ktcapsule::query(host,world,tag,physical,shape,{})==0;
}

inline void* releaseWorld(std::uintptr_t base,void* physical) {
    auto* world=*reinterpret_cast<void**>(base+ktcapsule::WorldGlobal);
    if(!world||*reinterpret_cast<std::uintptr_t*>(world)!=base+ktcapsule::WorldVtable||
       *reinterpret_cast<void**>(static_cast<unsigned char*>(physical)+0x68)!=world) return nullptr;
    return world;
}

inline bool surveyCrest(ktcapsule::Query host,void* world,const char* tag,void* physical,
                        ktcapsule::Vector crest,ktcapsule::Vector facing,CrestProfile& profile) {
    if(!finite(crest)||!finite(facing)||std::abs(facing.x*facing.x+facing.y*facing.y-1)>0.02f||
       std::abs(facing.z)>0.01f) return false;
    // End the downward casts near the original ledge. Stop at the first drop
    // or open column, or when the connected incline levels off.
    constexpr float spacing=0.06f,castTop=0.78f,castDepth=0.83f,maxRise=0.68f;
    float previous=0;
    unsigned level=0;
    for(unsigned i=0;i<=12;++i) {
        const float depth=i*spacing;
        ktcapsule::Shape sample{{crest.x+depth*facing.x,crest.y+depth*facing.y,crest.z+castTop},
                                {0,0,1},0.018f,0.01f};
        ++profile.samples;
        if(ktcapsule::query(host,world,tag,physical,sample,{})!=0) return false;
        const float hit=ktcapsule::query(host,world,tag,physical,sample,{0,0,-castDepth});
        if(!std::isfinite(hit)||hit<0||hit>castDepth) return false;
        if(hit==0) return i>0;
        const float rise=castTop-hit-sample.radius-sample.halfSpan;
        if(rise>maxRise||(!i&&std::abs(rise)>0.08f)||rise-previous>0.20f) return false;
        if(i&&previous-rise>0.055f) return true;
        if(rise>profile.rise) {
            profile.rise=rise;
            profile.depth=depth;
        }
        level=i&&std::abs(rise-previous)<0.015f?level+1:0;
        if(i>=4&&level>=2) return true;
        previous=rise;
    }
    return false;
}

inline bool crossingClear(ktcapsule::Query host,void* world,const char* tag,void* physical,
                          const ktvault::Dimensions& dims,const Record& record,const CrestProfile& profile,
                          ktcapsule::Vector position,bool pushing) {
    if(!releaseSweep(host,world,tag,physical,dims,position,record.crest.z+profile.headroom()-position.z))
        return false;
    const float z=pushing?position.z:record.crest.z+profile.pushHeight();
    ktcapsule::Shape capsule{{position.x,position.y,z+dims.center},{0,0,1},dims.size[0],dims.size[2]};
    const auto crest=record.crest;
    const auto facing=record.facing;
    const ktcapsule::Vector toCrown{crest.x+profile.depth*facing.x-position.x,crest.y+profile.depth*facing.y-position.y,0};
    if(ktcapsule::query(host,world,tag,physical,capsule,toCrown)!=0) return false;
    capsule.center.x+=toCrown.x;
    capsule.center.y+=toCrown.y;
    return ktcapsule::query(host,world,tag,physical,capsule,{})==0;
}

inline void readSupport(std::uintptr_t base,void* actor,Support& output) {
    __try {
        auto* physical=ktvault::physicalOwner(base,actor);
        if(!physical) return;
        const auto status=reinterpret_cast<ktcapsule::ReadStatus>(base+ktcapsule::Status);
        ktcapsule::PosePacket pose;
        if(status(physical,&pose)==1&&finite(pose.position)) {
            output.position=pose.position;
            output.positionKnown=true;
        }
        ktvault::LivingStatus living;
        if(status(physical,&living)==1&&(living.flying==0||living.flying==1)&&std::isfinite(living.groundHeight)) {
            output.groundHeight=living.groundHeight;
            output.flying=living.flying;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}

inline const char* supportState(const Support& sample,ktcapsule::Vector crest) {
    if(sample.flying<0) return "unknown";
    if(sample.flying==1) return "airborne";
    if(!sample.positionKnown) return "unknown";
    if(std::abs(sample.position.z-sample.groundHeight)>0.035f) return "contact_height_mismatch";
    const float dx=sample.position.x-crest.x,dy=sample.position.y-crest.y;
    return dx*dx+dy*dy<=0.18f*0.18f&&std::abs(sample.groundHeight-crest.z)<=0.035f?
        "grounded_near_crest":"grounded_elsewhere";
}
}
