#pragma once

#include <cstddef>
#include <cstdint>

namespace ktcapsule {
struct Vector {
    float x=0,y=0,z=0;
};
struct Shape {
    Vector center;
    Vector axis{0,0,1};
    float radius=0,halfSpan=0;
};

// Build 23914554 type 6 primitive packet. A null intersection override selects
// the worker's default overlap or sweep path based on the delta.
struct Packet {
    unsigned char header[0x18]{};
    std::uint32_t type=6;
    Shape* primitive=nullptr;
    unsigned char geometryOverride[0x64]{};
    Vector delta;
    std::uint32_t entityMask=0x10117;
    std::uint32_t padding9c=0;
    void** contacts=nullptr;
    std::uint32_t requiredGeometry=0,anyGeometry=3;
    void* intersectionOverride=nullptr;
    std::int32_t skipCount=0;
    void** skip=nullptr;
    std::uint32_t collisionType=0,collisionIgnore=0xc;
    unsigned char reserved[8]{};
    std::int32_t lockActive=0;
    void* lockState=&lockActive;
};
static_assert(sizeof(Shape)==0x20&&offsetof(Shape,radius)==0x18);
static_assert(sizeof(Packet)==0xe8&&offsetof(Packet,primitive)==0x20&&offsetof(Packet,delta)==0x8c);
static_assert(offsetof(Packet,entityMask)==0x98&&offsetof(Packet,anyGeometry)==0xac);
static_assert(offsetof(Packet,skipCount)==0xb8&&offsetof(Packet,skip)==0xc0&&offsetof(Packet,lockState)==0xe0);
}
