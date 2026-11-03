#pragma once
#include <cstdint>
#include <cstddef>
namespace ktguide {
constexpr unsigned Protocol=1,MaxDepth=4,MaxElements=65535;
struct Pin {const char* name;std::uintptr_t rva;unsigned char bytes[16];};
constexpr Pin Hooks[]={
 {"source",0x170ff94,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x18,0x55,0x41,0x56,0x41,0x57,0x48}},
 {"body",0x1331224,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x20,0x55,0x48,0x8b,0xec,0x48,0x83}},
 {"index",0x7e6a50,{0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec}}
};
constexpr std::uintptr_t BodyReturn=0x1331208,IndexReturn=0x13312ae;
// Source decompilation and selected caller sites establish void returns.
// Unused incoming register arguments are still forwarded unchanged.
using SourceFn=void (__fastcall *)(void*,const char*,std::uintptr_t,std::uintptr_t);
using BodyFn=void (__fastcall *)(void*,void*);
using IndexFn=void (__fastcall *)(void*);
enum class Result : unsigned {NotObserved,Matched,Unreadable,Editor,Bounds,Invalid,Changed,Caller,Owner,Mode};
constexpr const char* Results[]={"not_observed","observed","unreadable","editor","bounds","invalid","changed","caller","owner","mode"};
struct Digest {std::uint64_t ledges=14695981039346656037ull,structuralLedges=14695981039346656037ull,markers=14695981039346656037ull;std::uint32_t ledgeCount=0,markerCount=0;};
inline void hashByte(std::uint64_t& h,unsigned char b) {h^=b;h*=1099511628211ull;}
}
