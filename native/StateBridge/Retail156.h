#pragma once
#include <cstdint>
#include <cstring>

// Derived from original pinned retail bytes. See StateBridgeNativeEvidence.md.
namespace ktretail {
constexpr char Sha256[]="bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7";
constexpr std::uint32_t ImageSize=0x5b2d000;
constexpr std::uintptr_t Request=0x837e7c,PlayerReturn=0x82faa8,ScriptUpdate=0xa26b94;
constexpr std::uintptr_t ControllerVtable=0x3e74ac0,PlayerVtable=0x3aa4b98,EntityVtable=0x4072c78;
struct FunctionPin {std::uintptr_t rva;const char* first16;};
constexpr FunctionPin Pins[]={
    {Request,"48895c240848896c2410488974241857"},
    {ScriptUpdate,"48895c24184889742420574883ec3048"},
    {0xad496c,"48895c2408574883ec304d8bd0488bd9"}, // getfield
    {0xa26d60,"48895c2408574883ec304d8bd0488bd9"}, // setfield
    {0x715e00,"4883ec28e8e3120000488d0db0483303"}, // type
    {0x718a18,"48895c24084889742410574883ec2049"}, // tolstring
    {0x71805c,"4883ec28e887f0ffff83780807750c48"}, // touserdata
    {0xb9663c,"48895c24084889742410574883ec204c"}, // pushcclosure
    {0x713914,"48895c2408574883ec4033c0418bf844"}, // pcall
    {0xb96168,"48895c2408574883ec20488b4110488b"}, // checkstack
    {0x39a7d54,"48895c2408574883ec30488b02488bf9"}, // game frame-time wrapper
    {0x5d5f7c,"80796000741483fa01741385d2750538"}, // pausable game timer
};
inline bool bytesMatch(std::uintptr_t base,const FunctionPin& pin) {
    auto data=reinterpret_cast<const unsigned char*>(base+pin.rva);
    auto digit=[](char c)->unsigned {return c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);};
    for(unsigned i=0;i<16;++i) if(data[i]!=(digit(pin.first16[i*2])*16+digit(pin.first16[i*2+1]))) return false;
    return true;
}
}
