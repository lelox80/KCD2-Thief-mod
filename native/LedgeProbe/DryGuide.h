#pragma once

#include "Selection.h"
#include <cstring>

namespace ktledge {
constexpr unsigned DryLedgeCount=DryGuideObject+DryGuideCount,DryMarkerStart=27564;
constexpr unsigned DryMarkerCount=DryMarkerStart+2*DryGuideCount;
#ifdef KT_LEDGE_DRY_SELECTION
// Shipped Kuttenberg single-sided wide vault. The selector checks 0x200
// before the action slots, and the authored static side sets bit 0.
constexpr std::uint16_t DrySideFlags=0x207;
#else
constexpr std::uint16_t DrySideFlags=0x2; // Enabled for spatial lookup.
#endif
#ifdef KT_LEDGE_REPEATABLE
constexpr std::uint64_t DryStructuralHash=0x8998b547393cf584ull;
constexpr std::uint64_t DryMarkerHash=0x0a7c836408460e1beull;
constexpr std::uint64_t ActiveMarkerHash=0x30bb9d6bc133b9bbull;
#else
constexpr std::uint64_t DryStructuralHash=0x5c0798d6fdc4e5ddull;
#if defined(KT_LEDGE_TALL_PREVIEW)
constexpr std::uint64_t DryMarkerHash=0xe2a2234dce83d61dull;
#else
constexpr std::uint64_t DryMarkerHash=0x1e13f03b1aaa9503ull;
#endif
#endif
constexpr unsigned LedgeSize=16;
inline void dryTail(unsigned index,bool enabled,unsigned char (&tail)[LedgeSize]) {
    memset(tail,0,sizeof tail);
    const auto marker=static_cast<std::uint16_t>(DryMarkerStart+index*2);
    memcpy(tail+4,&marker,sizeof marker);
    tail[6]=2;
    if(enabled) memcpy(tail+8,&DrySideFlags,sizeof DrySideFlags);
}

inline void hashByte(std::uint64_t& digest,unsigned char byte) {
    digest=(digest^byte)*0x100000001b3ull;
}

enum class DryIndexResult : unsigned {
    ArmedOff, Caller, Manager, Editor, Counts, Tail, Structural, Markers, Fault, Matched, Count
};
constexpr const char* DryIndexNames[]={"armed_off","caller","manager","editor","counts","tail",
    "structural","markers","fault","matched"};

inline DryIndexResult checkDryMember(const unsigned char* ledges,unsigned ledgeCount,
                                    const unsigned char* markers,unsigned markerCount,bool allowEnabled=false) {
    if(ledgeCount!=DryLedgeCount||markerCount!=DryMarkerCount||!ledges||!markers)
        return DryIndexResult::Counts;
    for(unsigned index=0;index<DryGuideCount;++index) {
        const auto* record=ledges+(DryGuideObject+index)*LedgeSize;
        unsigned char inactive[LedgeSize],enabled[LedgeSize];
        dryTail(index,false,inactive);
        dryTail(index,true,enabled);
        if(memcmp(record,inactive,sizeof inactive)&&(!allowEnabled||memcmp(record,enabled,sizeof enabled)))
            return DryIndexResult::Tail;
    }
    std::uint64_t structural=0xcbf29ce484222325ull,marker=structural;
    for(unsigned index=0;index<ledgeCount;++index) {
        unsigned char normalized[LedgeSize];
        const auto* record=ledges+index*LedgeSize;
        if(index>=DryGuideObject) {
            dryTail(index-DryGuideObject,false,normalized);
            record=normalized;
        }
        for(unsigned byte=4;byte<16;++byte)
            hashByte(structural,record[byte]);
    }
    if(structural!=DryStructuralHash) return DryIndexResult::Structural;
    for(unsigned index=0;index<markerCount;++index)
        for(unsigned byte=0;byte<25;++byte) hashByte(marker,markers[index*28+byte]);
    return marker==DryMarkerHash?DryIndexResult::Matched:DryIndexResult::Markers;
}
}
