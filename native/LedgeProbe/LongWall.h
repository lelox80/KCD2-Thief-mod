#pragma once

#include "../ClearanceProbe/Capsule.h"
#include <cstring>

namespace ktlong {
constexpr ktcapsule::Vector Lip{2895.337402f,794.275635f,65.958130f};
constexpr ktcapsule::Vector Facing{-0.990268f,-0.139173f,0};
constexpr float HalfWidth=1.35f;
constexpr ktcapsule::Vector along(ktcapsule::Vector p,float side) {
    return {p.x+side*Facing.y,p.y-side*Facing.x,p.z};
}
constexpr ktcapsule::Vector Endpoints[]={along(Lip,-HalfWidth),along(Lip,HalfWidth)};

inline bool matches(const unsigned char* markers) {
    for(unsigned i=0;i<2;++i)
        if(memcmp(markers+i*28,&Endpoints[i],sizeof Endpoints[i])) return false;
    return true;
}

inline void seed(unsigned char* markers) {
    for(unsigned i=0;i<2;++i)
        memcpy(markers+i*28,&Endpoints[i],sizeof Endpoints[i]);
}
}
