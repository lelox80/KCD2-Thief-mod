#pragma once

#include "VaultTrial.h"

namespace ktair {
constexpr std::uint64_t WindowMs=750;
// The upper wall needs 0.71 m of rise. Leave room for the next physics sample.
constexpr float MaxObservedRise=0.78f;
enum class Step {Waiting, Rising, Ready, Expired, Lost};
struct Flight {
    float peakRise=0;
    bool airborne=false;

    Step observe(const ktvault::Contact& contact,ktcapsule::Vector launch,float ledgeZ,std::uint64_t age) {
        if(age>WindowMs) return Step::Expired;
        const auto p=contact.position;
        const float dx=p.x-launch.x,dy=p.y-launch.y,rise=p.z-launch.z;
        if(!(dx*dx+dy*dy<=0.35f*0.35f&&rise>=-0.04f&&rise<=MaxObservedRise)) return Step::Lost;
        if(!contact.flying) return airborne||!contact.grounded?Step::Lost:Step::Waiting;
        airborne=true;
        if(rise<peakRise-0.025f) return Step::Lost;
        if(rise>peakRise) peakRise=rise;
        const float remaining=ledgeZ-p.z;
        return rise>=0.10f&&remaining>=0.65f&&remaining<=1.49f?Step::Ready:Step::Rising;
    }
};

inline bool corridor(const ktvault::Wall& wall,ktcapsule::Vector pose,const float (&direction)[3],
                     ktcapsule::Corridor& route) {
    const float remaining=wall.lip.z-pose.z;
    if(!(remaining>=0.65f&&remaining<=1.49f)) return false;
    pose.z=wall.corridor.approach.z;
    return ktvault::approachRoute(wall,pose,direction,route);
}
}
