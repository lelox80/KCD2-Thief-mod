#pragma once

#include <cstdint>
#include <cstring>

namespace kttall {
constexpr std::uintptr_t ParamsRva=0x832310,SelectorReturn=0xf44b2e;
constexpr unsigned ParamsSize=0x318,VaultOntoMax=0x2c+4*0x44+0x3c;
constexpr float PreviewMax=1.70f;
using ParamBlock=std::uint32_t[ParamsSize/4];

// Preserve the original animation interval and every other action parameter.
inline bool extendHeight(ParamBlock& block) {
    auto* copy=reinterpret_cast<unsigned char*>(block);
    float originalMax=0;
    memcpy(&originalMax,copy+VaultOntoMax,sizeof originalMax);
    if(originalMax!=1.5f) return false;
    memcpy(copy+VaultOntoMax,&PreviewMax,sizeof PreviewMax);
    return true;
}
}
