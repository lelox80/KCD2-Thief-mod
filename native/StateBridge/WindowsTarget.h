#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Retail156.h"
#pragma comment(lib,"bcrypt.lib")

namespace ktwin {
inline bool sha256(const wchar_t* path,char (&hex)[65]) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char buffer[65536],digest[32];DWORD count=0;bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
       BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0) {
        ok=true;
        for(;;) {
            if(!ReadFile(file,buffer,sizeof buffer,&count,nullptr)) {ok=false;break;}
            if(!count) break;
            if(BCryptHashData(hash,buffer,count,0)<0) {ok=false;break;}
        }
        if(ok) ok=BCryptFinishHash(hash,digest,sizeof digest,0)>=0;
        if(ok) for(unsigned i=0;i<32;++i) sprintf_s(hex+i*2,65-i*2,"%02x",digest[i]);
    }
    if(hash) BCryptDestroyHash(hash);
    if(alg) BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(file);return ok;
}
inline DWORD verifyTarget(std::uintptr_t& base) {
    auto game=GetModuleHandleW(L"WHGame.dll");if(!game) return ERROR_MOD_NOT_FOUND;
    wchar_t path[32768];char hash[65]{};
    if(!GetModuleFileNameW(game,path,32768)||!sha256(path,hash)||strcmp(hash,ktretail::Sha256)) return ERROR_BAD_EXE_FORMAT;
    base=reinterpret_cast<std::uintptr_t>(game);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(game);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||nt->Signature!=IMAGE_NT_SIGNATURE||
       nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.SizeOfImage!=ktretail::ImageSize)
        return ERROR_BAD_EXE_FORMAT;
    for(const auto& pin:ktretail::Pins) if(!ktretail::bytesMatch(base,pin)) return ERROR_REVISION_MISMATCH;
    constexpr unsigned char caller[]={0x41,0xff,0xd0};
    if(memcmp(reinterpret_cast<void*>(base+ktretail::PlayerReturn-3),caller,3)||
       *reinterpret_cast<std::uintptr_t*>(base+ktretail::ControllerVtable+8)!=base+ktretail::Request)
        return ERROR_REVISION_MISMATCH;
    return ERROR_SUCCESS;
}
// A local read during a current host callback, never a retained actor pointer.
inline bool owner(std::uintptr_t base,void* controller,std::uint32_t& id,std::uint64_t& actorToken) {
    __try {
        auto c=reinterpret_cast<std::uintptr_t>(controller);
        if(*reinterpret_cast<std::uintptr_t*>(c)!=base+ktretail::ControllerVtable) return false;
        auto actor=*reinterpret_cast<std::uintptr_t*>(c+8);
        if(!actor||*reinterpret_cast<std::uintptr_t*>(actor)!=base+ktretail::PlayerVtable||
           *reinterpret_cast<std::uintptr_t*>(actor+0x180)!=c) return false;
        auto entity=*reinterpret_cast<std::uintptr_t*>(actor+0x38);
        if(!entity||*reinterpret_cast<std::uintptr_t*>(entity)!=base+ktretail::EntityVtable) return false;
        id=*reinterpret_cast<std::uint32_t*>(entity+0xc);
        if(!id||id!=*reinterpret_cast<std::uint32_t*>(actor+0x30)) return false;
        actorToken=actor;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
}
