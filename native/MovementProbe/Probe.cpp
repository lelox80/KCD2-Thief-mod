// Observation only: never changes a movement request or its return value.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include "MinHook.h"
#pragma comment(lib, "bcrypt.lib")

namespace {
constexpr uintptr_t kRequestRva = 0x837e7c, kPlayerReturnRva = 0x82faa8;
constexpr uintptr_t kControllerVtable = 0x3e74ac0;
constexpr char kHash[] = "bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7";
constexpr unsigned char kPrologue[] = {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
using RequestFn = bool (__fastcall *)(void*, void*);
RequestFn original = nullptr;
HMODULE ownModule = nullptr;
uintptr_t gameBase = 0;
void* hookTarget = nullptr;
HANDLE writerThread = nullptr, logFile = INVALID_HANDLE_VALUE;
std::atomic<bool> recording{false}, stopping{false};
std::atomic<bool> startAttempted{false}, stopInProgress{false};
std::atomic<unsigned> inFlight{0};
std::atomic<unsigned long long> dropped{0};
std::atomic<unsigned long long> calls{0}, matched{0}, unreadable{0};
SRWLOCK queueLock = SRWLOCK_INIT;
struct Sample { long long ticks; DWORD tid; unsigned long long flags; float speed, pseudoSpeed, x,y,z; };
constexpr unsigned kCapacity = 2048;
Sample queue[kCapacity];
unsigned head = 0, tail = 0;

bool sha256(const wchar_t* path, char (&hex)[65]) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char buffer[65536], digest[32]; DWORD count = 0; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        ok = true;
        for (;;) {
            if (!ReadFile(file, buffer, sizeof buffer, &count, nullptr)) { ok=false; break; }
            if (!count) break;
            if (BCryptHashData(hash, buffer, count, 0) < 0) { ok = false; break; }
        }
        if (ok) ok = BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0;
        if (ok) { for (unsigned i=0;i<32;++i) sprintf_s(hex+i*2, 65-i*2, "%02x", digest[i]); }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file); return ok;
}

bool readSample(void* controller, void* request, Sample& s) {
    // Validate only observed scalars; exceptions here must not block the real call.
    __try {
        if (*reinterpret_cast<uintptr_t*>(controller) != gameBase + kControllerVtable) return false;
        auto p = static_cast<const unsigned char*>(request);
        memcpy(&s.flags, p, 8);
        memcpy(&s.speed, p+0x38, 4); memcpy(&s.pseudoSpeed, p+0x3c, 4);
        memcpy(&s.x, p+0x7c, 4); memcpy(&s.y, p+0x80, 4); memcpy(&s.z, p+0x84, 4);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void enqueue(const Sample& sample) {
    if (!TryAcquireSRWLockExclusive(&queueLock)) { ++dropped; return; }
    unsigned next = (head+1)%kCapacity;
    if (next == tail) ++dropped;
    else { queue[head] = sample; head = next; }
    ReleaseSRWLockExclusive(&queueLock);
}

bool pop(Sample& sample) {
    AcquireSRWLockExclusive(&queueLock);
    bool available = tail != head;
    if (available) { sample = queue[tail]; tail = (tail+1)%kCapacity; }
    ReleaseSRWLockExclusive(&queueLock); return available;
}

bool __fastcall observedRequest(void* controller, void* request) {
    uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    ++inFlight;
    ++calls;
    if (recording.load(std::memory_order_acquire) && caller == gameBase+kPlayerReturnRva) {
        ++matched;
        Sample s{};
        if (readSample(controller, request, s)) {
            LARGE_INTEGER now; QueryPerformanceCounter(&now); s.ticks = now.QuadPart;
            s.tid = GetCurrentThreadId(); enqueue(s);
        } else ++unreadable;
    }
    // Keep the same pointer, arguments and result. No calls to Lua/engine getters.
    bool result = original(controller, request);
    --inFlight;
    return result;
}

void writeLine(const char* text) {
    DWORD written; WriteFile(logFile, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
}

DWORD WINAPI writer(void*) {
    char line[512]; Sample s;
    unsigned samples=0;
    for (;;) {
        bool any = false;
        while (pop(s)) {
            any = true;
            if (samples++ >= 12000) {
                recording=false;
                if (samples==12001) writeLine("{\"event\":\"capture_complete\",\"sample_limit\":12000}\n");
                continue;
            }
            // Undefined fields without their request flags are deliberately null.
            char speed[96] = "null", delta[160] = "null";
            if ((s.flags & 4) && std::isfinite(s.speed) && std::isfinite(s.pseudoSpeed))
                sprintf_s(speed, "[%.9g,%.9g]", s.speed, s.pseudoSpeed);
            if ((s.flags & 0x20) && std::isfinite(s.x) && std::isfinite(s.y) && std::isfinite(s.z))
                sprintf_s(delta, "[%.9g,%.9g,%.9g]", s.x,s.y,s.z);
            sprintf_s(line, "{\"event\":\"player_request\",\"qpc\":%lld,\"thread\":%lu,\"flags\":%llu,\"speed\":%s,\"delta\":%s}\n",
                      s.ticks,s.tid,s.flags,speed,delta);
            writeLine(line);
        }
        if (stopping.load() && !any) break;
        Sleep(20);
    }
    sprintf_s(line, "{\"event\":\"stopped\",\"dropped\":%llu,\"calls\":%llu,\"matched\":%llu,\"unreadable\":%llu}\n",
              dropped.load(),calls.load(),matched.load(),unreadable.load()); writeLine(line);
    FlushFileBuffers(logFile); return 0;
}
}

// Called explicitly after LoadLibrary returns. DllMain never installs a hook.
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
    // One attempt per process prevents concurrent initialization or restarting
    // over a retained trampoline. Restart the game before a new probe version.
    if (startAttempted.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    HMODULE game = GetModuleHandleW(L"WHGame.dll");
    if (!game) return ERROR_MOD_NOT_FOUND;
    wchar_t path[32768]; char hash[65]{};
    if (!GetModuleFileNameW(game, path, 32768) || !sha256(path, hash) || strcmp(hash,kHash)) return ERROR_BAD_EXE_FORMAT;
    gameBase = reinterpret_cast<uintptr_t>(game);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(game);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(gameBase + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt->OptionalHeader.SizeOfImage != 0x5b2d000)
        return ERROR_BAD_EXE_FORMAT;
    hookTarget = reinterpret_cast<void*>(gameBase+kRequestRva);
    if (memcmp(hookTarget,kPrologue,sizeof kPrologue)) return ERROR_REVISION_MISMATCH;
    // Also verify the actual local player call and controller virtual dispatch.
    constexpr unsigned char callSite[] = {0x41,0xff,0xd0};
    if (memcmp(reinterpret_cast<void*>(gameBase+kPlayerReturnRva-3),callSite,3) ||
        *reinterpret_cast<uintptr_t*>(gameBase+kControllerVtable+8) != gameBase+kRequestRva)
        return ERROR_REVISION_MISMATCH;
    // Pin our module: disabling the hook is supported; unloading code in flight is not.
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&KtProbeStart), &pinned)) return GetLastError();
    if (!GetModuleFileNameW(ownModule,path,32768)) return GetLastError();
    wchar_t* last = wcsrchr(path,L'\\'); if (!last) return ERROR_BAD_PATHNAME; *last = 0;
    wchar_t logPath[32768];
    swprintf_s(logPath,L"%s\\movement-probe-%lu-%llu.jsonl",path,GetCurrentProcessId(),GetTickCount64());
    logFile = CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (logFile == INVALID_HANDLE_VALUE) return GetLastError();
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    char start[256]; sprintf_s(start,"{\"event\":\"started\",\"mode\":\"observe-only\",\"qpc_frequency\":%lld,\"request_rva\":\"0x837e7c\"}\n",freq.QuadPart);
    writeLine(start);
    if (MH_Initialize() != MH_OK) { CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE; return ERROR_INVALID_FUNCTION; }
    if (MH_CreateHook(hookTarget,&observedRequest,reinterpret_cast<void**>(&original)) != MH_OK) {
        MH_Uninitialize(); CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE; return ERROR_INVALID_FUNCTION;
    }
    stopping=false; recording=true;
    writerThread=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
    if (!writerThread || MH_EnableHook(hookTarget) != MH_OK) {
        recording=false; stopping=true;
        if (writerThread) { WaitForSingleObject(writerThread,INFINITE); CloseHandle(writerThread); writerThread=nullptr; }
        MH_RemoveHook(hookTarget); MH_Uninitialize(); CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE;
        return ERROR_INVALID_FUNCTION;
    }
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
    if (stopInProgress.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    if (!writerThread) { stopInProgress=false; return ERROR_NOT_READY; }
    recording=false;
    auto disabled=MH_DisableHook(hookTarget);
    if (disabled != MH_OK && disabled != MH_ERROR_DISABLED) { stopInProgress=false; return ERROR_INVALID_FUNCTION; }
    // Retain trampoline and pinned DLL until process exit, even after disable.
    for (unsigned i=0; inFlight.load() && i<1000; ++i) Sleep(1);
    if (inFlight.load()) { stopInProgress=false; return ERROR_BUSY; }
    stopping=true; WaitForSingleObject(writerThread,INFINITE);
    CloseHandle(writerThread); writerThread=nullptr; CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE;
    stopInProgress=false;
    return ERROR_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) { ownModule=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
