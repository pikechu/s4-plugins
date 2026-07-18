#include "runtime/DiagnosticRuntime.h"

#include "diagnostics/ModuleInventory.h"
#include "runtime/StopRequest.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <stdexcept>
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

const char* StoreModeName(CompletionStoreMode mode) noexcept {
    switch (mode) {
        case CompletionStoreMode::Uninitialized: return "uninitialized";
        case CompletionStoreMode::WritableEmpty: return "writable-empty";
        case CompletionStoreMode::WritableLoaded: return "writable-loaded";
        case CompletionStoreMode::ReadOnlyBackup: return "read-only-backup";
        case CompletionStoreMode::ReadOnlyNormalizationFailed:
            return "read-only-normalization-failed";
        case CompletionStoreMode::ReadOnlyUnavailable:
            return "read-only-unavailable";
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
    header << "CampaignCompletionDebug bootstrap version=0.13.2 pid="
           << GetCurrentProcessId()
           << " mode=phase-7-classified-completion-manager";
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

    campaignDescriptors_ = AdmitCampaignDescriptorCatalog(*executable);
    std::ostringstream descriptorAdmission;
    descriptorAdmission
        << "campaign-descriptor-admission executable="
        << (campaignDescriptors_.executableAdmitted ? "admitted" : "rejected")
        << " addon="
        << (campaignDescriptors_.GroupAdmitted(CampaignDescriptorGroup::AddOn)
                ? "admitted" : "disabled")
        << " missioncd="
        << (campaignDescriptors_.GroupAdmitted(
                CampaignDescriptorGroup::MissionCd) ? "admitted" : "disabled")
        << " newworld="
        << (campaignDescriptors_.GroupAdmitted(
                CampaignDescriptorGroup::NewWorld) ? "admitted" : "disabled")
        << " newworld2="
        << (campaignDescriptors_.GroupAdmitted(
                CampaignDescriptorGroup::NewWorld2) ? "admitted" : "disabled")
        << " original="
        << (campaignDescriptors_.GroupAdmitted(
                CampaignDescriptorGroup::Original) ? "admitted" : "disabled")
        << " dark="
        << (campaignDescriptors_.GroupAdmitted(
                CampaignDescriptorGroup::DarkTribe) ? "admitted" : "disabled");
    logger_.Write(LogLevel::Info, descriptorAdmission.str());

    logger_.Write(
        LogLevel::Info,
        "phase-7-classified-completion-manager storage-writes=transactional "
        "manual-apply=revision-conflict-safe native-event=609-exact "
        "markers=enabled internal-menu-adapter=read-only");

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
        AbortStart();
        return false;
    }
    nativeRegistration_ =
        std::make_unique<NativeEventRegistration>(nativeAdmission_);

    try {
        campaignCapture_ = std::make_unique<CampaignMenuCapture>();
        campaignAssociation_ =
            std::make_unique<CampaignLaunchAssociation>();
        campaignSessionAdmission_ =
            std::make_unique<CampaignSessionAdmission>();
        fileOps_ = std::make_unique<Win32CompletionFileOps>();
        store_ = std::make_unique<CompletionStore>(
            *fileOps_, paths_->database, paths_->temporary, paths_->backup);
        const auto loaded = store_->Load();
        const auto snapshot = store_->Snapshot();
        markerIndex_.Publish(snapshot);
        const auto campaignSummary =
            campaignMarkerIndex_.Publish(snapshot, campaignDescriptors_);
        markerObserver_ =
            std::make_unique<FixedMapRowObserver>(markerIndex_);
        campaignMarkerObserver_ = std::make_unique<CampaignMarkerObserver>(
            campaignDescriptors_, campaignMarkerIndex_);
        markerSurface_ = std::make_unique<DirectDrawMarkerSurface>();
        if (!markerSurface_->Initialize()) {
            throw std::runtime_error("marker surface initialization failed");
        }
        markerRenderer_ = std::make_unique<CompletionMarkerRenderer>(
            *markerSurface_, [this](LogLevel level, std::string line) {
                logger_.Write(level, line);
            });
        fixedMapMenuMemory_ = AdmitFixedMapMenuMemory(*executable);
        std::ostringstream markerAdmission;
        markerAdmission << "completion-store mode="
                        << StoreModeName(loaded.mode)
                        << " records=" << loaded.recordCount
                        << " failure=" << static_cast<unsigned>(loaded.failure)
                        << " error=" << loaded.error
                        << " campaign-admitted=" << campaignSummary.admitted
                        << " campaign-rejected=" << campaignSummary.rejected
                        << " campaign-ambiguous=" << campaignSummary.ambiguous;
        logger_.Write(LogLevel::Info, markerAdmission.str());
        if (loaded.mode == CompletionStoreMode::Uninitialized ||
            loaded.mode == CompletionStoreMode::ReadOnlyUnavailable) {
            throw std::runtime_error("completion store unavailable");
        }

        worker_ = std::make_unique<CompletionWorker>(
            *store_, [this](LogLevel level, std::string line) {
                logger_.Write(level, line);
            }, [this](CompletionDatabaseSnapshot replacement) {
                markerIndex_.Publish(replacement);
                campaignMarkerIndex_.Publish(replacement,
                                             campaignDescriptors_);
            });
        if (!worker_->Start()) {
            throw std::runtime_error("completion worker failed to start");
        }
        completionManager_ = std::make_unique<CompletionManagerWindow>(
            module,
            [this] {
                CompletionManagerView view{};
                if (store_ == nullptr || paths_ == std::nullopt) {
                    return view;
                }
                const auto stored = store_->SnapshotWithRevision();
                const auto discovered = DiscoverFixedMaps(
                    paths_->pluginDirectory.parent_path());
                view.revision = stored.revision;
                view.storeMode = stored.mode;
                view.discoveryStatus = discovered.status;
                view.catalog = BuildCompletionManagerCatalog(
                    campaignDescriptors_, discovered.maps,
                    stored.database);
                return view;
            },
            [this](const ManualCompletionRequest& request) {
                if (store_ == nullptr) {
                    return ManualCompletionApplyResult{
                        ManualCompletionApplyStatus::Failed,
                        CompletionTransactionStage::None,
                        ERROR_INVALID_HANDLE, 0u};
                }
                const auto result = store_->ApplyManual(request);
                if (result.status ==
                    ManualCompletionApplyStatus::Committed) {
                    const auto snapshot = store_->Snapshot();
                    markerIndex_.Publish(snapshot);
                    campaignMarkerIndex_.Publish(
                        snapshot, campaignDescriptors_);
                }
                return result;
            });
        if (!completionManager_->Start()) {
            throw std::runtime_error(
                "completion manager UI thread failed to start");
        }
        completionCoordinator_ =
            std::make_unique<CompletionCandidateCoordinator>(
                [] { return CurrentUtcCompletionTime(); });
        completionAdmission_ = std::make_unique<CompletionAdmission>(
            *completionCoordinator_,
            [this](CompletionCandidate candidate) {
                const std::string stableId = candidate.record.stableId;
                const bool accepted =
                    worker_ != nullptr &&
                    worker_->TryEnqueue(std::move(candidate));
                logger_.Write(
                    accepted ? LogLevel::Info : LogLevel::Warning,
                    std::string("completion-admission ") +
                        (accepted ? "accepted" : "rejected") +
                        " stable-id=" + stableId);
                return accepted;
            });
    } catch (...) {
        logger_.Write(LogLevel::Error,
                      "campaign completion persistence initialization failed");
        AbortStart();
        return false;
    }

    if (!listeners_.Start(api_, logger_, *coordinator_, luaBridge_, origin_,
                          phase3Trace_, *campaignCapture_,
                          *campaignAssociation_, *campaignSessionAdmission_,
                          campaignDescriptors_, nativeSubscriber_,
                          victoryProbe_, *completionAdmission_,
                          *markerObserver_, *campaignMarkerObserver_,
                          *markerRenderer_, fixedMapMenuMemory_,
                          completionManager_.get())) {
        AbortStart();
        return false;
    }
    if (!nativeSubscriber_.Prepare(*nativeRegistration_, victoryProbe_)) {
        logger_.Write(LogLevel::Error, "native subscriber prepare failed");
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
    if (completionManager_ != nullptr) {
        completionManager_->SetMainMenuAvailable(false);
        if (!completionManager_->Stop(std::chrono::milliseconds(5000))) {
            logger_.Write(LogLevel::Warning,
                          "completion manager stop timed out; runtime retained");
            return false;
        }
    }
    if (completionAdmission_ != nullptr) completionAdmission_->Disable();
    if (campaignSessionAdmission_ != nullptr) {
        campaignSessionAdmission_->Disable();
    }
    nativeSubscriber_.RequestDetach();
    constexpr DWORD kDetachWaitStepMs = 10u;
    constexpr DWORD kDetachWaitLimitMs = 5'000u;
    DWORD waited = 0u;
    while (!nativeSubscriber_.Detached() && waited < kDetachWaitLimitMs) {
        Sleep(kDetachWaitStepMs);
        waited += kDetachWaitStepMs;
    }
    if (!nativeSubscriber_.Detached()) {
        logger_.Write(LogLevel::Warning,
                      "native subscriber detach timed out; runtime retained");
        return false;
    }

    if (!listenersStopped_) {
        if (coordinator_ != nullptr) {
            coordinator_->Disable();
        }
        origin_.Disable();
        if (campaignCapture_ != nullptr) campaignCapture_->Disable();
        if (campaignAssociation_ != nullptr) {
            campaignAssociation_->Disable();
        }
        if (markerObserver_ != nullptr) markerObserver_->Disable();
        if (campaignMarkerObserver_ != nullptr) {
            campaignMarkerObserver_->Disable();
        }
        if (markerRenderer_ != nullptr) markerRenderer_->Disable();
        victoryProbe_.Disable();
        const auto result = listeners_.Stop();
        listenerStopFailures_ = result.failures;
        listenersStopped_ = true;
        std::ostringstream summary;
        summary << "listeners stopped registered=" << result.registered
                << " removed=" << result.removed
                << " failures=" << result.failures;
        logger_.Write(LogLevel::Info, summary.str());
    }

    if (worker_ != nullptr &&
        !worker_->StopAndDrain(std::chrono::milliseconds(5000))) {
        logger_.Write(LogLevel::Warning,
                      "completion worker drain timed out; runtime retained");
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
    campaignSessionAdmission_.reset();
    completionAdmission_.reset();
    completionCoordinator_.reset();
    worker_.reset();
    completionManager_.reset();
    markerRenderer_.reset();
    if (markerSurface_ != nullptr) markerSurface_->Shutdown();
    markerSurface_.reset();
    campaignMarkerObserver_.reset();
    markerObserver_.reset();
    store_.reset();
    fileOps_.reset();
    nativeRegistration_.reset();
    nativeAdmission_ = {};
    fixedMapMenuMemory_ = {};
    campaignDescriptors_ = {};
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
        if (completionAdmission_ != nullptr) completionAdmission_->Disable();
        if (campaignSessionAdmission_ != nullptr) {
            campaignSessionAdmission_->Disable();
        }
        if (campaignCapture_ != nullptr) campaignCapture_->Disable();
        if (campaignAssociation_ != nullptr) {
            campaignAssociation_->Disable();
        }
        if (markerObserver_ != nullptr) markerObserver_->Disable();
        if (campaignMarkerObserver_ != nullptr) {
            campaignMarkerObserver_->Disable();
        }
        if (markerRenderer_ != nullptr) markerRenderer_->Disable();
        if (completionManager_ != nullptr) {
            completionManager_->SetMainMenuAvailable(false);
            if (!completionManager_->Stop(
                    std::chrono::milliseconds(5000))) {
                logger_.Write(
                    LogLevel::Warning,
                    "completion manager abort stop timed out; "
                    "runtime resources retained");
                return;
            }
        }
        victoryProbe_.Disable();
        listeners_.Stop();
        if (worker_ != nullptr) {
            worker_->StopAndDrain(std::chrono::milliseconds(5000));
        }
        completionAdmission_.reset();
        completionCoordinator_.reset();
        worker_.reset();
        completionManager_.reset();
        markerRenderer_.reset();
        if (markerSurface_ != nullptr) markerSurface_->Shutdown();
        markerSurface_.reset();
        campaignMarkerObserver_.reset();
        markerObserver_.reset();
        store_.reset();
        fileOps_.reset();
        campaignSessionAdmission_.reset();
        fixedMapMenuMemory_ = {};
        campaignAssociation_.reset();
        campaignCapture_.reset();
        campaignDescriptors_ = {};
        nativeRegistration_.reset();
        nativeAdmission_ = {};
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
