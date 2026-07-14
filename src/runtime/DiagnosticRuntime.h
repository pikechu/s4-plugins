#pragma once

#include "completion/CompletionAdmission.h"
#include "completion/CompletionStore.h"
#include "completion/CompletionWorker.h"
#include "completion/Win32CompletionFileOps.h"
#include "diagnostics/Logger.h"
#include "diagnostics/MarkerCalibrationTrace.h"
#include "diagnostics/Phase3Trace.h"
#include "identity/MapIdentityCoordinator.h"
#include "lua/LuaApi.h"
#include "lua/SuLuaMapBridge.h"
#include "marker/CompletionMarkerIndex.h"
#include "marker/FixedMapRowCalibration.h"
#include "native/NativeEventAdmission.h"
#include "native/NativeEventRegistration.h"
#include "native/NativeVictoryEventSubscriber.h"
#include "runtime/PluginPaths.h"
#include "runtime/S4Listeners.h"
#include "victory/LaunchOrigin.h"
#include "victory/SettlementUiProbe.h"
#include "victory/VictoryEventProbe.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>

namespace campaign_completion {

class DiagnosticRuntime final {
public:
    bool Start(HMODULE module);
    void RunControlLoop();
    void RequestControlledStop() noexcept;

private:
    bool TryControlledStop();
    void Stop();
    void AbortStart() noexcept;

    std::mutex mutex_;
    Phase3Trace phase3Trace_;
    Logger logger_;
    std::unique_ptr<Win32CompletionFileOps> fileOps_;
    std::unique_ptr<CompletionStore> store_;
    std::unique_ptr<MarkerCalibrationTrace> markerTrace_;
    std::unique_ptr<CompletionMarkerIndex> markerIndex_;
    std::unique_ptr<FixedMapRowCalibration> markerCalibration_;
    std::unique_ptr<CompletionWorker> worker_;
    std::unique_ptr<CompletionCandidateCoordinator> completionCoordinator_;
    std::unique_ptr<CompletionAdmission> completionAdmission_;
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
    bool listenersStopped_ = false;
    std::size_t listenerStopFailures_ = 0u;
    std::atomic<bool> controlledStopRequested_{false};
};

DWORD WINAPI BootstrapThread(void* module);
DiagnosticRuntime& RuntimeInstance();

}  // namespace campaign_completion
