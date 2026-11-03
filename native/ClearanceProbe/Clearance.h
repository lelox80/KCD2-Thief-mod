#pragma once
#include <cmath>
#include <cstdint>

namespace ktclearance {
constexpr std::uintptr_t Entry=0x8115bc, WorldGlobal=0x492d830, WorldVtable=0x407fc80, Tag=0x477af40;
constexpr std::uintptr_t Returns[]={0x36e6030,0x36e605d,0x36e60ae,0x36e60db};
constexpr unsigned char EntryBytes[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x18,0x48};
constexpr unsigned char CallBytes[]={0xff,0x90,0xc8,0x01,0,0};

struct Sweep {
    float center[3]{}, halfExtent[3]{}, delta[3]{};
};

inline unsigned direction(std::uintptr_t rva) {
    for(unsigned index=0;index<4;++index)
        if(Returns[index]==rva) return index+1;
    return 0;
}

inline bool valid(const Sweep& sweep,unsigned side) {
    if(side<1||side>4) return false;
    const unsigned axis=side<=2?0u:1u;
    for(unsigned index=0;index<3;++index) {
        if(!std::isfinite(sweep.center[index])||std::abs(sweep.center[index])>1e7f||
           !std::isfinite(sweep.halfExtent[index])||sweep.halfExtent[index]<0||sweep.halfExtent[index]>100||
           !std::isfinite(sweep.delta[index])||std::abs(sweep.delta[index])>100||
           (index!=axis&&sweep.delta[index]!=0)) return false;
    }
    return side%2?sweep.delta[axis]>=0:sweep.delta[axis]<=0;
}

// Mirrors only the adjustment caller's positive, strictly in-range test.
// Zero, endpoint, nonfinite and miss returns confer no clearance verdict.
inline bool usedByAdjustment(const Sweep& sweep,unsigned side,float result) {
    return valid(sweep,side)&&std::isfinite(result)&&result>0&&
        result<std::abs(sweep.delta[side<=2?0:1]);
}
}
