#pragma once

#include "diagnostics/Logger.h"
#include "hook/FixedMapLoadHook.h"
#include "hook/HlibCallPatchBackend.h"
#include "identity/FixedMapIdentityProbe.h"
#include "runtime/S4Listeners.h"

#include <windows.h>

#include <filesystem>
#include <mutex>
#include <memory>

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
    HlibCallPatchBackend hookBackend_;
    FixedMapLoadHook fixedMapHook_;
    std::unique_ptr<DirectOriginalLoadInvoker> originalInvoker_;
    std::unique_ptr<FixedMapIdentityProbe> probe_;
    S4API api_ = nullptr;
    std::filesystem::path stopRequestPath_;
    bool started_ = false;
    bool hookStarted_ = false;
};

DWORD WINAPI BootstrapThread(void* module);
DiagnosticRuntime& RuntimeInstance();

}  // namespace campaign_completion
