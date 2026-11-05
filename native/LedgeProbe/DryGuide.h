#pragma once

#include "Selection.h"
#include <cstring>
#ifdef KT_LEDGE_LONG_WALL
#include "LongWall.h"
#endif

namespace ktledge {
constexpr unsigned DryLedgeCount=DryGuideObject+DryGuideCount,DryMarkerStart=27564;
constexpr unsigned DryMarkerCount=DryMarkerStart+2*DryGuideCount;
#ifdef KT_LEDGE_RUNTIME_WALL
constexpr unsigned RuntimeMarkerStart=DryMarkerStart+2*FixedGuideCount;
constexpr std::uint16_t RuntimeSideFlags=0x205; // Omit the spatial insertion bit.
#else
constexpr unsigned FixedGuideCount=DryGuideCount;
#endif
#ifdef KT_LEDGE_DRY_SELECTION
// Shipped Kuttenberg single-sided wide vault. The selector checks 0x200
// before the action slots, and the authored static side sets bit 0.
constexpr std::uint16_t DrySideFlags=0x207;
#else
constexpr std::uint16_t DrySideFlags=0x2; // Enabled for spatial lookup.
#endif
#ifdef KT_LEDGE_REPEATABLE
#ifdef KT_LEDGE_RUNTIME_WALL
constexpr std::uint64_t DryStructuralHash=0x0b757213fc60bdba4ull;
constexpr std::uint64_t DryMarkerHash=0x07afd8b79157fa53eull;
#else
constexpr std::uint64_t DryStructuralHash=0x8998b547393cf584ull;
constexpr std::uint64_t DryMarkerHash=0x0a7c836408460e1beull;
#endif
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

inline std::uint64_t markerHash(const unsigned char* markers,unsigned start,unsigned count,bool normalize=false) {
    std::uint64_t hash=0xcbf29ce484222325ull;
#ifdef KT_LEDGE_LONG_WALL
    constexpr float Original[2][3]={{2895.372195f,794.028068f,65.958130f},{2895.302609f,794.523202f,65.958130f}};
#else
    (void)normalize;
#endif
    for(unsigned index=start;index<count;++index) {
        const auto* row=markers+index*28;
        for(unsigned byte=0;byte<25;++byte) {
#ifdef KT_LEDGE_LONG_WALL
            if(normalize&&index>=DryMarkerStart&&index<DryMarkerStart+2&&byte<sizeof Original[0]) {
                const auto* bytes=reinterpret_cast<const unsigned char*>(Original[index-DryMarkerStart]);
                hashByte(hash,bytes[byte]);
            } else
#endif
            hashByte(hash,row[byte]);
        }
    }
    return hash;
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
        if(memcmp(record,inactive,sizeof inactive)&&
           (!allowEnabled||index>=FixedGuideCount||memcmp(record,enabled,sizeof enabled)))
            return DryIndexResult::Tail;
    }
    std::uint64_t structural=0xcbf29ce484222325ull;
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
    bool normalize=false;
#ifdef KT_LEDGE_LONG_WALL
    normalize=allowEnabled&&ktlong::matches(markers+DryMarkerStart*28);
#endif
    return markerHash(markers,0,markerCount,normalize)==DryMarkerHash?
        DryIndexResult::Matched:DryIndexResult::Markers;
}
}
