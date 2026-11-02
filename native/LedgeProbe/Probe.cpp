// Selection observer only. The host alone owns output, action and movement.
#include "../StateBridge/WindowsTarget.h"
#include "../StateBridge/AuditQueue.h"
#include "Selection.h"
#include "MinHook.h"
#include <atomic>
#include <intrin.h>
#include <cwchar>

namespace {
ktledge::SelectionFn original=nullptr;
std::uintptr_t gameBase=0;
HMODULE ownModule=nullptr;
void* hookTarget=nullptr;
HANDLE writerThread=nullptr,logFile=INVALID_HANDLE_VALUE;
std::atomic<bool> attempted{false},recording{false},stopping{false},stopBusy{false};
std::atomic<bool> writeFailed{false};
std::atomic<unsigned> inFlight{0};
std::atomic<unsigned long long> calls{0},matched{0},unreadable{0},ownerRefused{0},dropped{0},reserved{0};
constexpr unsigned Capacity=2048,MaxRecords=12000;
struct Sample {
    long long ticks=0;
    DWORD thread=0;
    std::uint32_t entity=0;
    ktledge::Selection selection;
    bool readable=false,allowAlternate=false;
};
using Queue=kt::AuditQueue<Sample,Capacity>;
Queue queue;

bool selectionPins(std::uintptr_t base) {
    __try {
        return !memcmp(reinterpret_cast<void*>(base+ktledge::SelectionRva),ktledge::Prologue,sizeof ktledge::Prologue)&&
               !memcmp(reinterpret_cast<void*>(base+ktledge::CallerRva),ktledge::Caller,sizeof ktledge::Caller);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool ownerId(void* actor,std::uint32_t& id) {
    __try {
        auto a=reinterpret_cast<std::uintptr_t>(actor);
        if(!a) return false;
        auto controller=*reinterpret_cast<void**>(a+0x180);
        std::uint64_t verifiedActor=0;
        return ktwin::owner(gameBase,controller,id,verifiedActor)&&verifiedActor==a;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool readSelection(const ktledge::Selection* output,ktledge::Selection& value) {
    __try {memcpy(&value,output,sizeof value);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void enqueue(const Sample& sample) {
    auto count=reserved.load(std::memory_order_relaxed);
    bool budget=false;
    for(unsigned attempt=0;attempt<64;++attempt) {
        if(count>=MaxRecords) {recording=false;return;}
        if(reserved.compare_exchange_weak(count,count+1,std::memory_order_relaxed)) {budget=true;break;}
    }
    if(!budget) {++dropped;recording=false;return;}
    Queue::Ticket ticket;
    if(queue.reserve(ticket)!=Queue::Result::Reserved) {++dropped;recording=false;return;}
    queue.commit(ticket,sample);
    if(count+1==MaxRecords) recording=false;
}
void capture(void* actor,const ktledge::Selection* output,bool alternate) {
    Sample s;
    if(!ownerId(actor,s.entity)) {++ownerRefused;return;}
    s.readable=readSelection(output,s.selection);
    if(!s.readable) ++unreadable;
    LARGE_INTEGER time;QueryPerformanceCounter(&time);
    s.ticks=time.QuadPart;s.thread=GetCurrentThreadId();s.allowAlternate=alternate;
    enqueue(s);
}
void observeReturned(std::uintptr_t caller,void* actor,const ktledge::Selection* output,bool alternate) {
    if(recording.load(std::memory_order_acquire)&&caller==gameBase+ktledge::ReturnRva) {
        ++matched;capture(actor,output,alternate);
    }
}
void __fastcall observedSelection(void* actor,ktledge::Selection* output,bool alternate) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++inFlight;++calls;
    // Keep original exceptions outside any observation exception handler. Finally
    // decrements the counter on normal return, host SEH and C++ unwinding.
    __try {
        original(actor,output,alternate);
        observeReturned(caller,actor,output,alternate);
    } __finally {--inFlight;}
}
bool writeLine(const char* line) {
    DWORD written=0;
    const auto length=static_cast<DWORD>(strlen(line));
    if(!WriteFile(logFile,line,length,&written,nullptr)||written!=length) {
        recording=false;writeFailed=true;return false;
    }
    return true;
}
DWORD WINAPI writer(void*) {
    Sample s;char line[512];
    for(;;) {
        while(queue.pop(s)) {
            if(writeFailed.load()) continue;
            if(s.readable) {
                sprintf_s(line,"{\"event\":\"ledge_selection\",\"qpc\":%lld,\"thread\":%lu,\"entity\":%u,\"allow_alternate\":%s,\"readable\":true,\"action_type\":%d,\"handle\":%u,\"candidate\":%s}\n",
                    s.ticks,s.thread,s.entity,s.allowAlternate?"true":"false",s.selection.actionType,s.selection.handle,
                    s.selection.actionType!=-1&&s.selection.handle!=0xffffffffu?"true":"false");
            } else {
                sprintf_s(line,"{\"event\":\"ledge_selection\",\"qpc\":%lld,\"thread\":%lu,\"entity\":%u,\"allow_alternate\":%s,\"readable\":false,\"action_type\":null,\"handle\":null,\"candidate\":null}\n",
                    s.ticks,s.thread,s.entity,s.allowAlternate?"true":"false");
            }
            writeLine(line);
        }
        if(stopping.load(std::memory_order_acquire)) break;
        Sleep(20);
    }
    sprintf_s(line,"{\"event\":\"stopped\",\"calls\":%llu,\"matched\":%llu,\"owner_refused\":%llu,\"unreadable\":%llu,\"dropped\":%llu,\"reserved\":%llu,\"record_limit\":%u,\"write_failed\":%s}\n",
        calls.load(),matched.load(),ownerRefused.load(),unreadable.load(),dropped.load(),reserved.load(),MaxRecords,writeFailed.load()?"true":"false");
    if(!writeFailed.load()) writeLine(line);
    FlushFileBuffers(logFile);return 0;
}
}

extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
    if(attempted.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    DWORD error=ktwin::verifyTarget(gameBase);if(error) return error;
    if(!selectionPins(gameBase)) return ERROR_REVISION_MISMATCH;
    hookTarget=reinterpret_cast<void*>(gameBase+ktledge::SelectionRva);
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&KtProbeStart),&pinned)) return GetLastError();
    wchar_t path[32768],logPath[32768];
    if(!GetModuleFileNameW(ownModule,path,32768)) return GetLastError();
    auto last=wcsrchr(path,L'\\');if(!last) return ERROR_BAD_PATHNAME;*last=0;
    swprintf_s(logPath,L"%s\\ledge-probe-%lu-%llu.jsonl",path,GetCurrentProcessId(),GetTickCount64());
    logFile=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(logFile==INVALID_HANDLE_VALUE) return GetLastError();
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    char line[384];sprintf_s(line,"{\"event\":\"started\",\"mode\":\"observe-only\",\"probe\":\"ledge-selection\",\"qpc_frequency\":%lld,\"selection_rva\":\"0x831eac\",\"return_rva\":\"0x831ccd\",\"record_limit\":%u}\n",frequency.QuadPart,MaxRecords);
    if(!writeLine(line)) {CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_WRITE_FAULT;}
    if(MH_Initialize()!=MH_OK) {CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;}
    if(MH_CreateHook(hookTarget,&observedSelection,reinterpret_cast<void**>(&original))!=MH_OK) {
        MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;return ERROR_INVALID_FUNCTION;
    }
    stopping=false;recording=true;
    writerThread=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
    if(!writerThread||MH_EnableHook(hookTarget)!=MH_OK) {
        recording=false;stopping=true;
        if(writerThread) {WaitForSingleObject(writerThread,INFINITE);CloseHandle(writerThread);writerThread=nullptr;}
        MH_RemoveHook(hookTarget);MH_Uninitialize();CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
        return ERROR_INVALID_FUNCTION;
    }
    return ERROR_SUCCESS;
}
extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
    if(stopBusy.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    if(!writerThread) {stopBusy=false;return ERROR_NOT_READY;}
    recording=false;
    auto disabled=MH_DisableHook(hookTarget);
    if(disabled!=MH_OK&&disabled!=MH_ERROR_DISABLED) {stopBusy=false;return ERROR_INVALID_FUNCTION;}
    // Keep trampoline and module pinned even after disable: a callback may have
    // entered immediately before MinHook suspended its thread.
    for(unsigned i=0;inFlight.load()&&i<1000;++i) Sleep(1);
    if(inFlight.load()) {stopBusy=false;return ERROR_BUSY;}
    stopping=true;
    if(WaitForSingleObject(writerThread,5000)!=WAIT_OBJECT_0) {stopBusy=false;return ERROR_BUSY;}
    CloseHandle(writerThread);writerThread=nullptr;CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;
    stopBusy=false;return ERROR_SUCCESS;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {ownModule=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
