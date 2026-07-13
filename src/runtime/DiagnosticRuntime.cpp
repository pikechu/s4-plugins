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

    std::wstring modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length == 0 || length == static_cast<DWORD>(modulePath.size())) {
        return false;
    }
    modulePath.resize(length);
    const auto pluginDirectory = std::filesystem::path(modulePath).parent_path();
    const auto gameDirectory = pluginDirectory.parent_path();
    const auto campaignDirectory = gameDirectory / L"CampaignCompletion";
    const auto logPath = campaignDirectory / L"CampaignCompletion.log";
    const auto iniPath = campaignDirectory / L"CampaignCompletionDebug.ini";
    if (!logger_.Open(logPath)) {
        return false;
    }

    std::ostringstream header;
    header << "CampaignCompletionDebug bootstrap version=0.2.5 pid="
           << GetCurrentProcessId() << " identity-mode=su-lua-read-only";
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
        static_cast<DWORD>(std::size(traceRoot)), iniPath.c_str());
    if (traceLength > 0u &&
        traceLength < static_cast<DWORD>(std::size(traceRoot) - 1u)) {
        captureTrace_.Open(traceRoot, GetCurrentProcessId());
    }

    coordinator_ = std::make_unique<MapIdentityCoordinator>(
        [this](std::string record) {
            logger_.Write(LogLevel::Info, record);
        },
        [this](std::string record) { captureTrace_.Write(record); });

    if (!listeners_.Start(api_, logger_, *coordinator_, luaBridge_)) {
        api_->Release();
        api_ = nullptr;
        coordinator_.reset();
        captureTrace_.Close();
        logger_.Close();
        return false;
    }

    stopRequestPath_ = gameDirectory / L"CampaignCompletion" /
                       L"CampaignCompletionDebug.stop";
    started_ = true;
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
            Stop();
            return;
        }
        Sleep(kControlIntervalMs);
    }
}

void DiagnosticRuntime::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
        return;
    }
    if (coordinator_ != nullptr) {
        coordinator_->Disable();
    }
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
    captureTrace_.Write(
        result.failures == 0u ? "controlled-stop-flush=success"
                              : "controlled-stop-flush=failed");
    captureTrace_.Close();
    logger_.Close();
    coordinator_.reset();
    started_ = false;
}

DWORD WINAPI BootstrapThread(void* module) {
    auto& runtime = RuntimeInstance();
    if (runtime.Start(static_cast<HMODULE>(module))) {
        runtime.RunControlLoop();
    }
    return 0;
}

}  // namespace campaign_completion
