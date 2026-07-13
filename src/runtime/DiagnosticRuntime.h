#pragma once

#include "diagnostics/Logger.h"
#include "runtime/S4Listeners.h"

#include <windows.h>

#include <filesystem>
#include <mutex>

namespace campaign_completion {

class DiagnosticRuntime final {
public:
    bool Start(HMODULE module);
    void RunControlLoop();
    void Stop();

private:
    std::mutex mutex_;
    Logger logger_;
    S4Listeners listeners_;
    S4API api_ = nullptr;
    std::filesystem::path stopRequestPath_;
    bool started_ = false;
};

DWORD WINAPI BootstrapThread(void* module);
DiagnosticRuntime& RuntimeInstance();

}  // namespace campaign_completion
