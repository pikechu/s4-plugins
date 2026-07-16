#include "runtime/DiagnosticRuntime.h"

#include "diagnostics/ModuleInventory.h"
#include "runtime/StopRequest.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace campaign_completion {
namespace {

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

bool EqualInsensitive(const std::wstring& left, const wchar_t* right) {
    std::wstring lowered = left;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lowered == right;
}

std::string CompatibilityName(CompatibilityResult result) {
    switch (result) {
        case CompatibilityResult::Compatible:
            return "compatible";
        case CompatibilityResult::VersionMismatch:
            return "version-mismatch";
        case CompatibilityResult::HashMismatch:
            return "hash-mismatch";
    }
    return "unknown";
}

}  // namespace

DiagnosticRuntime& RuntimeInstance() {
    static DiagnosticRuntime* const runtime = new DiagnosticRuntime();
    return *runtime;
}

bool DiagnosticRuntime::Start(HMODULE module) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_ || module == nullptr) {
        return false;
    }

    paths_ = ResolvePluginPaths(module);
    if (!paths_.has_value()) {
        return false;
    }
    if (!logger_.Open(paths_->log)) {
        return false;
    }

    std::ostringstream header;
    header << "CampaignCompletionDebug bootstrap version=0.8.2 pid="
           << GetCurrentProcessId()
           << " mode=all-campaign-public-catalog-calibration";
    logger_.Write(LogLevel::Info, header.str());
    constexpr DWORD kModuleInventoryRetryCount = 20u;
    constexpr DWORD kModuleInventoryRetryDelayMs = 100u;
    auto modules = EnumerateLoadedModules();
    const auto hasExecutable = [](const std::vector<ModuleInfo>& inventory) {
        return std::any_of(inventory.begin(), inventory.end(),
                           [](const ModuleInfo& loaded) {
                               return EqualInsensitive(loaded.name,
                                                       L"s4_main.exe");
                           });
    };
    for (DWORD attempt = 1u;
         attempt < kModuleInventoryRetryCount && !hasExecutable(modules);
         ++attempt) {
        Sleep(kModuleInventoryRetryDelayMs);
        modules = EnumerateLoadedModules();
    }
    const ModuleInfo* executable = nullptr;
    for (const auto& loaded : modules) {
        std::ostringstream line;
        line << "module name=" << Utf8(loaded.name) << " base=0x" << std::hex
             << loaded.baseAddress << std::dec << " size=" << loaded.size
             << " path=" << Utf8(loaded.path.wstring());
        if (!loaded.version.empty()) {
            line << " version=" << loaded.version;
        }
        if (!loaded.sha256.empty()) {
            line << " sha256=" << loaded.sha256;
        }
        logger_.Write(LogLevel::Info, line.str());
        if (EqualInsensitive(loaded.name, L"s4_main.exe")) {
            executable = &loaded;
        }
    }
    if (executable == nullptr) {
        logger_.Write(LogLevel::Error, "S4_Main.exe absent from module inventory");
        logger_.Close();
        return false;
    }

    const auto compatibility = CheckTargetExecutable(*executable);
    logger_.Write(compatibility == CompatibilityResult::Compatible
                      ? LogLevel::Info
                      : LogLevel::Error,
                  "executable compatibility=" + CompatibilityName(compatibility));
    if (compatibility != CompatibilityResult::Compatible) {
        logger_.Close();
        return false;
    }

    logger_.Write(LogLevel::Info,
                  "phase-6b-read-only storage=disabled native-events=disabled "
                  "markers=disabled internal-menu-adapter=disabled");

    constexpr DWORD kWaitStepMs = 100;
    constexpr DWORD kWaitLimitMs = 30'000;
    DWORD waited = 0;
    while (GetModuleHandleW(L"S4ModApi.dll") == nullptr && waited < kWaitLimitMs) {
        Sleep(kWaitStepMs);
        waited += kWaitStepMs;
    }
    if (GetModuleHandleW(L"S4ModApi.dll") == nullptr) {
        logger_.Write(LogLevel::Error, "S4ModApi.dll was not loaded within 30 seconds");
        logger_.Close();
        return false;
    }

    api_ = S4ApiCreate();
    if (api_ == nullptr) {
        logger_.Write(LogLevel::Error, "S4ApiCreate failed");
        logger_.Close();
        return false;
    }

    wchar_t traceRoot[32768]{};
    const DWORD traceLength = GetPrivateProfileStringW(
        L"Diagnostic", L"CaptureTraceRoot", L"", traceRoot,
        static_cast<DWORD>(std::size(traceRoot)), paths_->ini.c_str());
    if (traceLength > 0u &&
        traceLength < static_cast<DWORD>(std::size(traceRoot) - 1u)) {
        if (!phase3Trace_.Open(traceRoot, GetCurrentProcessId())) {
            logger_.Write(LogLevel::Warning,
                          "phase 3 trace root was not admitted");
        }
    }

    coordinator_ = std::make_unique<MapIdentityCoordinator>(
        [this](std::string record) {
            logger_.Write(LogLevel::Info, record);
        },
        [this](std::string record) {
            phase3Trace_.Write(Phase3TraceChannel::Identity, record);
        });
    try {
        campaignCapture_ = std::make_unique<CampaignMenuCapture>();
        campaignAssociation_ =
            std::make_unique<CampaignLaunchAssociation>();
    } catch (...) {
        logger_.Write(LogLevel::Error,
                      "campaign menu diagnostic initialization failed");
        AbortStart();
        return false;
    }

    if (!listeners_.Start(api_, logger_, *coordinator_, luaBridge_, origin_,
                          phase3Trace_, *campaignCapture_,
                          *campaignAssociation_)) {
        AbortStart();
        return false;
    }

    stopRequestPath_ = paths_->stop;
    started_ = true;
    listenersStopped_ = false;
    listenerStopFailures_ = 0u;
    controlledStopRequested_.store(false, std::memory_order_release);
    logger_.Write(LogLevel::Info, "diagnostic runtime started");
    return true;
}

void DiagnosticRuntime::RunControlLoop() {
    constexpr DWORD kControlIntervalMs = 100;
    while (true) {
        std::filesystem::path requestPath;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!started_) {
                return;
            }
            requestPath = stopRequestPath_;
        }

        if (ConsumeStopRequest(requestPath)) {
            logger_.Write(LogLevel::Info, "controlled stop requested");
            RequestControlledStop();
        }
        if (controlledStopRequested_.load(std::memory_order_acquire) &&
            TryControlledStop()) {
            return;
        }
        Sleep(kControlIntervalMs);
    }
}

void DiagnosticRuntime::RequestControlledStop() noexcept {
    controlledStopRequested_.store(true, std::memory_order_release);
}

bool DiagnosticRuntime::TryControlledStop() {
    if (!listenersStopped_) {
        if (coordinator_ != nullptr) {
            coordinator_->Disable();
        }
        origin_.Disable();
        if (campaignCapture_ != nullptr) campaignCapture_->Disable();
        if (campaignAssociation_ != nullptr) {
            campaignAssociation_->Disable();
        }
        const auto result = listeners_.Stop();
        listenerStopFailures_ = result.failures;
        listenersStopped_ = true;
        std::ostringstream summary;
        summary << "listeners stopped registered=" << result.registered
                << " removed=" << result.removed
                << " failures=" << result.failures;
        logger_.Write(LogLevel::Info, summary.str());
    }

    Stop();
    return true;
}

void DiagnosticRuntime::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
        return;
    }
    if (api_ != nullptr) {
        api_->Release();
        api_ = nullptr;
    }
    logger_.Write(LogLevel::Info, "diagnostic runtime stopped");
    phase3Trace_.Write(
        Phase3TraceChannel::Decision,
        listenerStopFailures_ == 0u ? "controlled-stop-flush=success"
                                    : "controlled-stop-flush=failed");
    campaignAssociation_.reset();
    campaignCapture_.reset();
    coordinator_.reset();
    phase3Trace_.Close();
    logger_.Close();
    paths_.reset();
    started_ = false;
    listenersStopped_ = false;
    listenerStopFailures_ = 0u;
    controlledStopRequested_.store(false, std::memory_order_release);
}

void DiagnosticRuntime::AbortStart() noexcept {
    try {
        if (campaignCapture_ != nullptr) campaignCapture_->Disable();
        if (campaignAssociation_ != nullptr) {
            campaignAssociation_->Disable();
        }
        listeners_.Stop();
        campaignAssociation_.reset();
        campaignCapture_.reset();
        if (api_ != nullptr) {
            api_->Release();
            api_ = nullptr;
        }
        coordinator_.reset();
        phase3Trace_.Close();
        logger_.Close();
        paths_.reset();
        listenersStopped_ = false;
        listenerStopFailures_ = 0u;
    } catch (...) {
    }
}

DWORD WINAPI BootstrapThread(void* module) {
    auto& runtime = RuntimeInstance();
    if (runtime.Start(static_cast<HMODULE>(module))) {
        runtime.RunControlLoop();
    }
    return 0;
}

}  // namespace campaign_completion
