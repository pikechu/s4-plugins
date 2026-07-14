#include "runtime/DiagnosticRuntime.h"

#include "diagnostics/ModuleInventory.h"
#include "runtime/StopRequest.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

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
    static DiagnosticRuntime runtime;
    return runtime;
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
    header << "CampaignCompletionDebug bootstrap version=0.3.3 pid="
           << GetCurrentProcessId()
           << " mode=native-event-calibration";
    logger_.Write(LogLevel::Info, header.str());
    const auto modules = EnumerateLoadedModules();
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

    constexpr DWORD kAdmissionWaitStepMs = 100u;
    constexpr DWORD kAdmissionWaitLimitMs = 30'000u;
    DWORD admissionWaited = 0u;
    do {
        nativeAdmission_ = NativeEventAdmission::Create(*executable);
        if (nativeAdmission_ ||
            nativeAdmission_.failure !=
                NativeEventAdmissionFailure::EngineUnavailable) {
            break;
        }
        Sleep(kAdmissionWaitStepMs);
        admissionWaited += kAdmissionWaitStepMs;
    } while (admissionWaited < kAdmissionWaitLimitMs);
    if (!nativeAdmission_) {
        logger_.Write(LogLevel::Error,
                      "native event admission failed code=" +
                          std::to_string(static_cast<unsigned>(
                              nativeAdmission_.failure)));
        api_->Release();
        api_ = nullptr;
        coordinator_.reset();
        phase3Trace_.Close();
        logger_.Close();
        return false;
    }
    nativeRegistration_ =
        std::make_unique<NativeEventRegistration>(nativeAdmission_);

    if (!listeners_.Start(api_, logger_, *coordinator_, luaBridge_, origin_,
                          settlement_, nativeSubscriber_, victoryProbe_,
                          phase3Trace_)) {
        api_->Release();
        api_ = nullptr;
        nativeRegistration_.reset();
        nativeAdmission_ = {};
        coordinator_.reset();
        phase3Trace_.Close();
        logger_.Close();
        return false;
    }
    if (!nativeSubscriber_.Prepare(*nativeRegistration_, victoryProbe_)) {
        const auto listenerResult = listeners_.Stop();
        logger_.Write(LogLevel::Error,
                      "native subscriber prepare failed listener-failures=" +
                          std::to_string(listenerResult.failures));
        api_->Release();
        api_ = nullptr;
        nativeRegistration_.reset();
        nativeAdmission_ = {};
        coordinator_.reset();
        phase3Trace_.Close();
        logger_.Close();
        return false;
    }

    stopRequestPath_ = paths_->stop;
    started_ = true;
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
    nativeSubscriber_.RequestDetach();
    constexpr DWORD kDetachWaitStepMs = 10u;
    constexpr DWORD kDetachWaitLimitMs = 5'000u;
    DWORD waited = 0u;
    while (!nativeSubscriber_.Detached() && waited < kDetachWaitLimitMs) {
        Sleep(kDetachWaitStepMs);
        waited += kDetachWaitStepMs;
    }
    if (!nativeSubscriber_.Detached()) {
        phase3Trace_.Write(Phase3TraceChannel::Decision,
                           "controlled-stop-flush=failed");
        logger_.Write(LogLevel::Warning,
                      "native subscriber detach timed out; runtime retained");
        return false;
    }
    Stop();
    return true;
}

void DiagnosticRuntime::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
        return;
    }
    if (coordinator_ != nullptr) {
        coordinator_->Disable();
    }
    origin_.Disable();
    settlement_.Disable();
    victoryProbe_.Disable();
    const auto result = listeners_.Stop();
    if (api_ != nullptr) {
        api_->Release();
        api_ = nullptr;
    }
    std::ostringstream summary;
    summary << "listeners stopped registered=" << result.registered
            << " removed=" << result.removed
            << " failures=" << result.failures;
    logger_.Write(LogLevel::Info, summary.str());
    logger_.Write(LogLevel::Info, "diagnostic runtime stopped");
    phase3Trace_.Write(
        Phase3TraceChannel::Decision,
        result.failures == 0u ? "controlled-stop-flush=success"
                              : "controlled-stop-flush=failed");
    phase3Trace_.Close();
    logger_.Close();
    nativeRegistration_.reset();
    nativeAdmission_ = {};
    coordinator_.reset();
    paths_.reset();
    started_ = false;
    controlledStopRequested_.store(false, std::memory_order_release);
}

DWORD WINAPI BootstrapThread(void* module) {
    auto& runtime = RuntimeInstance();
    if (runtime.Start(static_cast<HMODULE>(module))) {
        runtime.RunControlLoop();
    }
    return 0;
}

}  // namespace campaign_completion
