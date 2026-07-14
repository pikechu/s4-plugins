#pragma once

#include "diagnostics/Phase3Trace.h"
#include "diagnostics/Logger.h"
#include "identity/MapIdentityCoordinator.h"
#include "lua/LuaApi.h"
#include "lua/SuLuaMapBridge.h"
#include "native/NativeEventAdmission.h"
#include "native/NativeEventRegistration.h"
#include "native/NativeVictoryEventSubscriber.h"
#include "runtime/S4Listeners.h"
#include "runtime/PluginPaths.h"
#include "victory/LaunchOrigin.h"
#include "victory/SettlementUiProbe.h"
#include "victory/VictoryEventProbe.h"

#include <windows.h>

#include <filesystem>
#include <atomic>
#include <mutex>
#include <memory>

namespace campaign_completion {

class DiagnosticRuntime final {
public:
    bool Start(HMODULE module);
    void RunControlLoop();
    void RequestControlledStop() noexcept;

private:
    bool TryControlledStop();
    void Stop();

    std::mutex mutex_;
    Phase3Trace phase3Trace_;
    Logger logger_;
    S4Listeners listeners_;
    S4LuaApi luaApi_;
    S4LuaMapBridge luaBridge_{luaApi_};
    std::unique_ptr<MapIdentityCoordinator> coordinator_;
    LaunchOriginTracker origin_;
    SettlementUiProbe settlement_;
    NativeEventAdmission nativeAdmission_{};
    std::unique_ptr<NativeEventRegistration> nativeRegistration_;
    NativeVictoryEventSubscriber nativeSubscriber_;
    VictoryEventProbe victoryProbe_;
    S4API api_ = nullptr;
    std::optional<PluginPaths> paths_;
    std::filesystem::path stopRequestPath_;
    bool started_ = false;
    std::atomic<bool> controlledStopRequested_{false};
};

DWORD WINAPI BootstrapThread(void* module);
DiagnosticRuntime& RuntimeInstance();

}  // namespace campaign_completion
