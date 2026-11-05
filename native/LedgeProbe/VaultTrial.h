#pragma once

#include "../ClearanceProbe/CapsuleControl.h"
#ifdef KT_LEDGE_LONG_WALL
#include "LongWall.h"
#endif

namespace ktvault {
constexpr std::uint32_t Handle=4129u<<16;
constexpr std::uintptr_t PhysicalGetter=0x8a49fc,LivingVtable=0x3a82570,GetParams=0x3d5384;
#ifdef KT_LEDGE_TALL_PREVIEW
constexpr auto Corridor=ktcapsule::TallWall;
constexpr auto Approach=Corridor.approach;
constexpr ktcapsule::Vector Facing{-0.990268f,-0.139173f,0};
#else
constexpr auto Corridor=ktcapsule::RaisedPavement;
constexpr ktcapsule::Vector Approach{2897.10f,766.46f,61.34f};
constexpr ktcapsule::Vector Facing{0,1,0};
#endif
#ifdef KT_LEDGE_REPEATABLE
struct Wall {
    std::uint32_t handle;
    ktcapsule::Corridor corridor;
    ktcapsule::Vector facing,lip;
    float halfWidth;
    bool crestOnly=false;
};
constexpr Wall Walls[]={
#ifdef KT_LEDGE_LONG_WALL
    {Handle,ktcapsule::TallWall,Facing,ktlong::Lip,ktlong::HalfWidth},
#else
    {Handle,ktcapsule::TallWall,Facing,{2895.337402f,794.275635f,65.958130f},0.25f},
#endif
    {Handle+(1u<<16),ktcapsule::RaisedPavement,{0,1,0},{2897.121826f,767.061523f,62.353310f},0.30f},
};

inline const Wall* wallFor(std::uint32_t handle) {
    for(const auto& wall:Walls) if(wall.handle==handle) return &wall;
    return nullptr;
}

inline bool aim(const Wall& wall,const float (&direction)[3]) {
    const auto facing=wall.facing;
    const float length=direction[0]*direction[0]+direction[1]*direction[1];
    return std::abs(length-1)<0.02f&&std::abs(direction[2])<=0.01f&&
        direction[0]*facing.x+direction[1]*facing.y>=0.94f;
}

inline bool approachRoute(const Wall& wall,ktcapsule::Vector p,const float (&direction)[3],
                          ktcapsule::Corridor& route,bool recovered=false) {
    if(!aim(wall,direction)) return false;
    if(recovered&&wall.handle!=Handle) return false;
    const auto f=wall.facing;
    const float x=p.x-wall.lip.x,y=p.y-wall.lip.y;
    const float distance=-(x*f.x+y*f.y);
    const float side=x*f.y-y*f.x;
    const float alongAim=side+distance*(direction[0]*f.y-direction[1]*f.x)/
        (direction[0]*f.x+direction[1]*f.y);
    float lane=0;
#ifdef KT_LEDGE_LONG_WALL
    if(wall.handle==Handle) {
        // The native target projects the original actor onto the segment.
        // Keep a margin at the ends for the local face and top survey.
        if(!(distance>=0.36f&&distance<=1.55f&&std::abs(side)<=wall.halfWidth-0.22f&&
             std::abs(alongAim)<=wall.halfWidth+0.07f&&std::abs(p.z-wall.corridor.approach.z)<0.12f))
            return false;
        lane=side;
    } else
#endif
    {
    // The retry can pick up a nearby lane. Both paths still land within the
    // authored strip, and run clearance from the real physical position.
    const float maxDistance=recovered?1.45f:1.28f;
    const float maxSide=recovered?0.68f:wall.halfWidth+0.16f;
    if(!(distance>=0.45f&&distance<=maxDistance&&std::abs(side)<=maxSide&&
         std::abs(p.z-wall.corridor.approach.z)<0.12f)) return false;
    if(std::abs(alongAim)>(recovered?0.70f:wall.halfWidth+0.07f)) return false;
    const float projected=wall.crestOnly?side:alongAim;
    lane=projected<-wall.halfWidth?-wall.halfWidth:
        projected>wall.halfWidth?wall.halfWidth:projected;
    }
    route=wall.corridor;
    const float landingSide=(route.landing.x-wall.lip.x)*f.y-(route.landing.y-wall.lip.y)*f.x;
    route.landing.x+=(lane-landingSide)*f.y;
    route.landing.y-=(lane-landingSide)*f.x;
    return true;
}

inline bool tallRetryInRange(const float (&position)[3],const float (&direction)[3]) {
    // This standing lookup uses the physical origin plus the 1.05 m center.
    // The real origin and capsule are independently checked before admission.
    const ktcapsule::Vector feet{position[0],position[1],position[2]-1.05f};
    ktcapsule::Corridor route;
    return approachRoute(Walls[0],feet,direction,route,true);
}
constexpr float TallLookupAnchor[]={2896.32568f,794.443054f,65.36646f};
constexpr float TallLookupDirection[]={Facing.x,Facing.y,0};
#endif
enum class Result : unsigned {NotRun, Physical, Ground, Stance, Position, Clearance, Released, WallSection,
                              LandingSupport, Overlap};
constexpr const char* ResultNames[]={"not_run","physical","ground","stance","approach","clearance","released",
                                     "wall_section","landing_support","overlap"};
struct LivingStatus {
    std::int32_t type=2,flying=-1;
    unsigned char fields[0x1c]{};
    ktcapsule::Vector velocity;
    unsigned char motionTail[0x18]{};
    float groundHeight=0;
    unsigned char tail[0x100-0x4c]{};
};
static_assert(offsetof(LivingStatus,velocity)==0x24&&offsetof(LivingStatus,groundHeight)==0x48);
struct Dimensions {
    std::uint32_t type=1;
    float pivot=0,eye=0,size[3]{},center=0;
    unsigned char fields[0x18]{};
    std::uint32_t capsule=0;
    float groundContact=0;
};
static_assert(sizeof(Dimensions)==0x3c&&offsetof(Dimensions,capsule)==0x34);
inline bool standingCapsule(const Dimensions& d) {
    return d.capsule==1&&std::abs(d.pivot)<0.01f&&std::abs(d.center-1.05f)<0.01f&&
           std::abs(d.size[0]-0.35f)<0.01f&&std::abs(d.size[1]-0.35f)<0.01f&&std::abs(d.size[2]-0.40f)<0.01f;
}
struct Contact {
    ktcapsule::Vector position;
    bool grounded=false,flying=false;
};

inline bool pins(std::uintptr_t base) {
    return ktcapsule::pins(base)&&
        *reinterpret_cast<std::uintptr_t*>(base+LivingVtable+0x28)==base+GetParams&&
        *reinterpret_cast<std::uintptr_t*>(base+LivingVtable+0x30)==base+ktcapsule::Status&&
        ktretail::bytesMatch(base,{GetParams,"488bc44889580848896810574883ec40"})&&
        ktretail::bytesMatch(base,{PhysicalGetter,"40534883ec20ba01000000488bd9e8b5"})&&
        ktretail::bytesMatch(base,{0x83022c,"8b4b1833d28ac1d0e980e1012401884c"})&&
        ktretail::bytesMatch(base,{0x1533840,"488bc448895808488968104889701848"});
}

inline bool inApproach(ktcapsule::Vector p) {
    const float x=p.x-Approach.x,y=p.y-Approach.y;
    return std::abs(x*Facing.x+y*Facing.y)<0.10f&&std::abs(x*Facing.y-y*Facing.x)<0.24f&&
        std::abs(p.z-Approach.z)<0.12f;
}

inline bool clear(const ktcapsule::Control& control,bool airborne=false) {
    if(!(airborne?(control.complete&&control.atPose==0):ktcapsule::groundControlsPass(control))||
       !control.routeComplete) return false;
    for(float n:control.overlaps) if(n!=0) return false;
    for(float n:control.sweeps) if(n!=0) return false;
    return true;
}

inline void* physicalOwner(std::uintptr_t base,void* actor) {
    const auto entity=*reinterpret_cast<void**>(static_cast<unsigned char*>(actor)+0x38);
    using GetPhysical=void* (__fastcall *)(void*);
    auto physical=reinterpret_cast<GetPhysical>(base+PhysicalGetter)(entity);
    return physical&&*reinterpret_cast<std::uintptr_t*>(physical)==base+LivingVtable?physical:nullptr;
}

inline Result inspect(std::uintptr_t base,void* actor,Contact& contact) {
    __try {
        auto physical=physicalOwner(base,actor);
        if(!physical) return Result::Physical;
        const auto getStatus=reinterpret_cast<ktcapsule::ReadStatus>(base+ktcapsule::Status);
        LivingStatus status;
        if(getStatus(physical,&status)!=1||(status.flying!=0&&status.flying!=1))
            return Result::Ground;
        Dimensions d;
        if(reinterpret_cast<ktcapsule::ReadStatus>(base+GetParams)(physical,&d)!=1||!standingCapsule(d))
            return Result::Stance;
        ktcapsule::PosePacket pose;
        if(getStatus(physical,&pose)!=1) return Result::Position;
#ifdef KT_LEDGE_REPEATABLE
        const auto p=pose.position;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) return Result::Position;
#else
        if(!inApproach(pose.position)) return Result::Position;
#endif
        contact.position=pose.position;
        contact.flying=status.flying==1;
        contact.grounded=!contact.flying&&std::abs(status.groundHeight-pose.position.z)<0.035f;
        return contact.grounded||contact.flying?Result::Released:Result::Ground;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return Result::Physical;}
}

inline bool immediateTakeoff(const Contact& current,ktcapsule::Vector previous,std::uint64_t elapsedMs) {
    const auto p=current.position;
    return current.flying&&elapsedMs<=100&&std::abs(p.x-previous.x)<0.045f&&
        std::abs(p.y-previous.y)<0.045f&&p.z>=previous.z&&p.z-previous.z<0.05f;
}

inline Result checkClearance(std::uintptr_t base,void* actor,const ktcapsule::Corridor& route=Corridor,
                             const ktcapsule::LedgeSection* section=nullptr,bool airborne=false,
                             const ktcapsule::Vector* adjustedTarget=nullptr) {
    __try {
        auto physical=physicalOwner(base,actor);
        if(!physical) return Result::Physical;
        Dimensions d;
        if(reinterpret_cast<ktcapsule::ReadStatus>(base+GetParams)(physical,&d)!=1||!standingCapsule(d))
            return Result::Stance;
        ktcapsule::Control control;
#ifdef KT_LEDGE_REPEATABLE
        const bool requireSupport=!section||!section->crestOnly;
#else
        constexpr bool requireSupport=false;
#endif
        const bool known=ktcapsule::run(base,physical,d.center,d.size[0],d.size[2],control,true,route,
                                        requireSupport,section,airborne,adjustedTarget);
        if(known&&control.sectionRefused) return Result::WallSection;
        if(known&&control.landingRefused) return Result::LandingSupport;
        if(known&&control.complete&&control.atPose!=0) return Result::Overlap;
        return known&&clear(control,airborne)?Result::Released:Result::Clearance;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return Result::Physical;}
}
}
