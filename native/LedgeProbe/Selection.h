#pragma once
#include <cstddef>
#include <cstdint>

// Static inference from the original pinned retail 1.5.6 DLL, not a public API.
// This pair describes a candidate. Subsequent host checks may reject the climb.
// See docs/KCD2LedgeInterface.md. Never invoke this entry to request movement.
namespace ktledge {
constexpr unsigned Protocol=4;
constexpr std::uintptr_t AttemptRva=0x831b20,AttemptCallerRva=0x830263,AttemptReturnRva=0x830268;
constexpr std::uintptr_t SelectionRva=0x831eac;
constexpr std::uintptr_t CallerRva=0x831cc8,ReturnRva=0x831ccd;
constexpr unsigned char Prologue[]={
    0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,
    0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55};
constexpr unsigned char Caller[]={0xe8,0xdf,0x01,0x00,0x00};
constexpr std::uintptr_t QueryRva=0x832188,QueryCallerRva=0x8320f5,QueryReturnRva=0x8320fa;
#ifdef KT_LEDGE_DRY_GUARD
// The retail resolver decodes the high word as object index and the low word
// as segment and side. Reserve the one appended object for this dry fixture.
constexpr std::uint32_t DryGuideObject=4129,NoGuide=0xffffffffu;
#ifdef KT_LEDGE_RUNTIME_WALL
constexpr unsigned RuntimeGuideCount=64,FixedGuideCount=2;
constexpr unsigned DryGuideCount=FixedGuideCount+RuntimeGuideCount;
#elif defined(KT_LEDGE_REPEATABLE)
constexpr unsigned DryGuideCount=2;
#else
constexpr unsigned DryGuideCount=1;
#endif
constexpr std::uint32_t dryQueryResult(std::uint32_t handle) {
    return (handle>>16)-DryGuideObject<DryGuideCount?NoGuide:handle;
}
#ifdef KT_LEDGE_RUNTIME_WALL
constexpr unsigned RuntimeGuideObject=DryGuideObject+2;
constexpr std::uint32_t runtimeHandle(unsigned slot) {return (RuntimeGuideObject+slot)<<16;}
constexpr bool runtimeObject(std::uint32_t handle) {
    return (handle>>16)-RuntimeGuideObject<RuntimeGuideCount;
}
constexpr bool exactRuntimeHandle(std::uint32_t handle) {
    return runtimeObject(handle)&&(handle&0xffff)==0;
}
#endif
#endif
constexpr std::uintptr_t FrameworkRva=0x85612c,FrameworkCallerRva=0x175e08b,FrameworkReturnRva=0x175e090;
constexpr std::uintptr_t ManagerGlobalRva=0x5484b38,LedgeActionVtableRva=0x3c0d880;
constexpr std::uintptr_t LedgeActionSecondary10VtableRva=0x3c0d840,LedgeActionSecondary50VtableRva=0x3c0d808;
// Canonical Cry actor owns a separate WH C_ActionActor, with a back-pointer.
constexpr std::uintptr_t ActionActorVtableRva=0x3b098e0,PlayerActionActorOffset=0x280,ActionActorPlayerOffset=0x28;
struct HookPin {const char* name;std::uintptr_t rva;unsigned char bytes[16];};
constexpr HookPin Hooks[]={
    {"attempt",AttemptRva,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55}},
    {"selection",SelectionRva,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x55}},
    {"query",QueryRva,{0x40,0x53,0x48,0x83,0xec,0x40,0x48,0x8b,0xc1,0x48,0x8b,0xda,0x48,0x83,0xc1,0x18}},
    {"framework",FrameworkRva,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48}}
};
struct CallerPin {std::uintptr_t rva;unsigned char bytes[5];};
constexpr CallerPin Callers[]={
    {AttemptCallerRva,{0xe8,0xb8,0x18,0x00,0x00}},
    {CallerRva,{0xe8,0xdf,0x01,0x00,0x00}},
    {QueryCallerRva,{0xe8,0x8e,0x00,0x00,0x00}},
    {FrameworkCallerRva,{0xe8,0x9c,0x80,0x0f,0xff}}
};
#ifdef KT_LEDGE_DRY_GUARD
constexpr CallerPin SecondQueryCaller={0x89efa2,{0xe8,0xe1,0x31,0xf9,0xff}};
#endif
struct Selection {
    std::int32_t actionType=-1;
    std::uint32_t handle=0xffffffffu;
};
struct Request {Selection selection;unsigned char flags[4]{};};
static_assert(sizeof(Selection)==8&&offsetof(Selection,handle)==4,"retail output layout");
static_assert(sizeof(Request)==12&&offsetof(Request,flags)==8,"retail request copy");
#if defined(_MSC_VER)
using SelectionFn=void (__fastcall *)(void* actor,Selection* output,bool allowAlternate);
// Only RDX, low R9W and the fifth stack pointer are consumed by Attempt.
// Forward the two opaque register slots and full R9 unchanged as well.
using AttemptFn=std::uint8_t (__fastcall *)(std::uintptr_t,void*,std::uintptr_t,std::uintptr_t,void*);
using QueryFn=std::uint32_t* (__fastcall *)(void*,std::uint32_t*,const float*,const float*,float,float,float,float);
// AL is preserved byte-for-byte. Only 0/1 are classified as a boolean result.
using FrameworkFn=std::uint8_t (__fastcall *)(void*,void*);
#else
using SelectionFn=void (*)(void* actor,Selection* output,bool allowAlternate);
#endif
// RCX actor, RDX output, R8B flag; no incoming stack arguments or return value.
// Only the native caller at CallerRva is currently established by direct xrefs.
// Owner reads during the current callback: actor+0x180 -> controller, then
// ktwin::owner checks controller vtable, controller+8 == actor, player vtable,
// actor+0x38 entity vtable, entity+0xc ID == actor+0x30 ID != 0.
// None of those pointer values belongs in a retained sample or log.
}
