namespace {
DWORD startProbe() {
    if(attempted.exchange(true)) return ERROR_ALREADY_INITIALIZED;
    const auto error=ktwin::verifyTarget(gameBase);
    if(error) return error;
    if(!pins()) return ERROR_REVISION_MISMATCH;
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&startProbe),&pinned)) return GetLastError();
    wchar_t path[32768],logPath[32768];
    const auto size=GetModuleFileNameW(module,path,32768);
    if(!size||size>=32768) return ERROR_BAD_PATHNAME;
    auto separator=wcsrchr(path,L'\\');
    if(!separator) return ERROR_BAD_PATHNAME;
    *separator=0;
    if(swprintf_s(logPath,L"%s\\%s-%lu-%llu.jsonl",path,CaptureName,GetCurrentProcessId(),GetTickCount64())<0)
        return ERROR_BAD_PATHNAME;
    capture=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(capture==INVALID_HANDLE_VALUE) return GetLastError();
    auto target=reinterpret_cast<void*>(gameBase+Entry);
    DWORD result=ERROR_WRITE_FAULT;
    if(writeLine(Header)) {
        result=ERROR_INVALID_FUNCTION;
        if(MH_Initialize()==MH_OK) {
            if(MH_CreateHook(target,reinterpret_cast<void*>(&observed),reinterpret_cast<void**>(&original))==MH_OK) {
                recording=true;
                if(MH_EnableHook(target)==MH_OK) return ERROR_SUCCESS;
                recording=false;
                MH_RemoveHook(target);
            }
            MH_Uninitialize();
        }
    }
    CloseHandle(capture);
    capture=INVALID_HANDLE_VALUE;
    return result;
}

DWORD stopProbe() {
    DWORD result=ERROR_NOT_READY;
    if(capture!=INVALID_HANDLE_VALUE) {
        recording=false;
        const auto status=MH_DisableHook(reinterpret_cast<void*>(gameBase+Entry));
        result=ERROR_INVALID_FUNCTION;
        if(status==MH_OK||status==MH_ERROR_DISABLED) {
            for(unsigned i=0;inFlight&&i<1000;++i) Sleep(1);
            result=ERROR_BUSY;
            if(!inFlight) {
                result=saveCapture()?ERROR_SUCCESS:ERROR_WRITE_FAULT;
                CloseHandle(capture);
                capture=INVALID_HANDLE_VALUE;
            }
        }
    }
    // Keep the trampoline and this pinned module alive until normal game exit.
    return result;
}
}

extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStart(void*) {
    if(!TryAcquireSRWLockExclusive(&lifecycle)) return ERROR_BUSY;
    const auto result=startProbe();
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}

extern "C" __declspec(dllexport) DWORD WINAPI KtProbeStop(void*) {
    if(!TryAcquireSRWLockExclusive(&lifecycle)) return ERROR_BUSY;
    const auto result=stopProbe();
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {module=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
