#pragma once
#include <cstdint>
namespace ktguidesource {
constexpr unsigned MaxDepth=4;
struct Pin {const char* name;std::uintptr_t rva;unsigned char bytes[16];};
constexpr Pin Hooks[]={
 {"source",0x170ff94,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x18,0x55,0x41,0x56,0x41,0x57,0x48}},
 {"lookup",0xf41730,{0x48,0x83,0xec,0x28,0x48,0x8b,0x91,0x08,0x01,0x00,0x00,0x48,0x85,0xd2,0x74,0x2b}}
};
constexpr std::uintptr_t LookupReturn=0x46405d,ProviderLookup=0xf41774,ProviderVtable=0x3a97328;
using SourceFn=void (__fastcall *)(void*,const char*,std::uintptr_t,std::uintptr_t);
using LookupFn=const char* (__fastcall *)(void*);
enum class Result : unsigned {NotObserved,Caller,PathUnknown,MemberMismatch,Unreadable,Changed,ProviderUnknown,Empty,Truncated,BadPath,Observed};
constexpr const char* Results[]={"not_observed","caller","path_unknown","member_mismatch","unreadable","changed","provider_unknown","empty","truncated","bad_path","archive_observed"};
constexpr const char* Namespaces[]={"unknown","levels","data_levels","other"};
constexpr const char* Archives[]={"unknown","base_level_pak","owned_mod_pak","other_pak","other_path"};
}
