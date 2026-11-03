#pragma once

namespace {
std::uintptr_t gameBase=0;
HMODULE module=nullptr;
HANDLE capture=INVALID_HANDLE_VALUE;
std::atomic<bool> attempted{false},recording{false};
SRWLOCK lifecycle=SRWLOCK_INIT;
std::atomic<unsigned> inFlight{0};
bool writeLine(const char* line) {
    DWORD written=0;
    const auto size=static_cast<DWORD>(strlen(line));
    return WriteFile(capture,line,size,&written,nullptr)&&written==size;
}

}
