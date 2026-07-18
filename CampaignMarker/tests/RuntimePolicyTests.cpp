#include "diagnostics/ModuleInventory.h"
#include "runtime/CallbackGate.h"
#include "runtime/S4Listeners.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

}  // namespace

int RunRuntimePolicyTests() {
    using namespace campaign_completion;

    CallbackGate gate;
    Require(gate.TryEnter(), "running gate accepts callbacks");
    gate.Leave();
    gate.CloseAndWait();
    Require(!gate.TryEnter(), "closed gate rejects callbacks");

    CallbackGate blockingGate;
    Require(blockingGate.TryEnter(), "blocking test enters gate");
    std::atomic<bool> closeCompleted{false};
    std::thread closer([&blockingGate, &closeCompleted] {
        blockingGate.CloseAndWait();
        closeCompleted.store(true);
    });
    while (blockingGate.TryEnter()) {
        blockingGate.Leave();
        std::this_thread::yield();
    }
    const bool completedWhileCallbackInFlight = closeCompleted.load();
    blockingGate.Leave();
    closer.join();
    Require(!completedWhileCallbackInFlight && closeCompleted.load(),
            "close waits for in-flight callbacks");

    RateLimiter limiter(1000u);
    Require(limiter.Allow(0u) && !limiter.Allow(999u) && limiter.Allow(1000u),
            "rate limiter preserves its interval boundary");

    ModuleInfo approved{};
    approved.name = L"S4_Main.exe";
    approved.version = "2.50.1516.0";
    approved.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    Require(CheckTargetExecutable(approved) == CompatibilityResult::Compatible,
            "exact executable identity is accepted");
    approved.sha256.back() = '0';
    Require(CheckTargetExecutable(approved) == CompatibilityResult::HashMismatch,
            "executable hash mutation fails closed");

    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto policy =
        ReadText(root / "config" / "CampaignCompletionDebug.ini");
    for (const auto* required : {
             "Version=0.13.0",
             "DiagnosticMode=Phase7ClassifiedCompletionManager",
             "InternalMenuReadOnly=1", "InternalMenuWrites=0",
             "InternalMenuRendering=0", "PublicMarkerFallback=0",
             "PublicSettlementUiProbe=0", "LaunchOriginTracking=1",
             "LoadOriginRecovery=0", "NativeEventSubscription=1",
             "InternalVictoryHook=0", "HookCount=0", "CodePatchBytes=0",
             "GameDefaultGameEndCheckCalls=0", "LuaWrites=0",
             "GameDataWrites=0", "CompletionDetection=1",
             "CompletionStorage=1", "CompletionMarkers=1",
             "CompletionDatabaseReadOnly=0",
             "CompletionDatabaseWrites=1", "CampaignMarkers=1",
             "CampaignMarkerFrameCapacity=36",
             "CampaignMenuPublicCapture=1",
             "CampaignMenuPages=3,4,5,6,11,12,13,14,15,16,17,18,19,20,21",
             "CampaignMenuAssociation=SettlersUnitedLua",
             "CampaignMenuCache=PreFrameMissionControlResidency",
             "CampaignMenuControlFilter=ExactPhase6DStaticMissionIds",
             "CampaignMenuEntryPrime=PublicScreenReadBeforeFirstUiFrame",
             "CampaignLaunchLeaseMs=30000",
             "CampaignDescriptorCatalog=ImmutableStaticReadOnly",
             "CampaignDescriptorIdentity=SameSessionRelativeExact",
             "CampaignDescriptorGroups=All107Closed",
             "CampaignDescriptorEvidenceGaps=None",
             "CampaignSessionAdmission=ExactDescriptorSameSession",
             "CampaignVictoryTransaction=OneExactLocalEvent609",
             "Enabled=1", "OpenHotkey=Ctrl+Shift+M",
             "ClassifiedCampaigns=1", "ClassifiedFixedMaps=1",
             "ManualAtomicApply=1", "RandomRecordsReadOnly=1",
             "RevisionConflictProtection=1",
             "IdentitySource=SettlersUnitedLua", "CaptureTraceRoot="}) {
        Require(policy.find(required) != std::string::npos,
                "Phase 7 packaged policy field is missing");
    }
    Require(policy.find("CaptureTraceRoot=F:") == std::string::npos,
            "packaged trace root remains empty");

    const auto runtime =
        ReadText(root / "src" / "runtime" / "DiagnosticRuntime.cpp");
    const auto runtimeHeader =
        ReadText(root / "src" / "runtime" / "DiagnosticRuntime.h");
    const auto listeners =
        ReadText(root / "src" / "runtime" / "S4Listeners.cpp");
    const auto listenerHeader =
        ReadText(root / "src" / "runtime" / "S4Listeners.h");
    const auto campaignCapture =
        ReadText(root / "src" / "campaign" / "CampaignMenuCapture.cpp");
    const auto campaignAssociation = ReadText(
        root / "src" / "campaign" / "CampaignLaunchAssociation.cpp");
    const auto campaignDescriptors = ReadText(
        root / "src" / "campaign" / "CampaignDescriptorCatalog.cpp");
    const auto campaignSessionAdmission = ReadText(
        root / "src" / "campaign" / "CampaignSessionAdmission.cpp");
    for (const auto* frozenWindow : {
             "f096f8969a8304498b0cd053adbb6c5a7793e77eb317f9c591b4c41f67dcb36d",
             "09408e191881efa8ba44705bb66a358e3e83d3e362d288ffed6a3596e4b98971",
             "9459c9d0fc6c027ec736eb9bf9b3c0ddf6642edc0e8957919406bc8f81812799",
             "147f9db65cbdd0b09fba7a0001cb15c96bea8cba4855226c6ebfca98c59b90b3",
             "c34714420d42d8f680705dd2d88dd158eb92ace78bcbe2b601a7eba8788e141b",
             "63278d29a609391708defde9fbbf28fd9c09d9f8a1ac9521fd8beab849aef8b8",
             "85f8bbd64886596ca14e3e9f5e624c91b30785331e2e4b243abedf33455d1e47",
             "12d6217ca1e1354ac4ff4b9b2d8dbf521c743bcdf5e3205f20c090b59d38372a",
             "d37b56be5c1509afac177a56b17f9a9849b1ebf25cc696de848fb6d74a1c5c56",
             "e1374d0d00cdd2d2280ea963907cf6f9692198c449862fd59895e80e2e3c4b50",
             "08595043d6f859af73f4f92311023d60b1da1ebaf86f80c2e47b556741945471",
             "3c561626b14678bf2a94d76097c44b785a91dd520a0e6d4438cecb6e1576ce88",
             "ded0825cc53c46669bc2b8ba8e2bf19085b81129554609e81d035ce724d769b3",
             "685185042a2f72803083ffc09df07b225812a2a8569704e78e3908b9e3e95722",
             "96a78fc162b178d98a4733fc758b0cc1fb1e73ba75ce51bd26b5a4308a6bd978",
             "44b14df6007177e510e7f68311f7827ead6cf7d9f3ae8271f9daa60cfeeae016",
             "e0a84e09d81d061ea4a948ee3a97c86dbfc6d74f14a1ce8f138a6ef17964df34"}) {
        Require(campaignDescriptors.find(frozenWindow) != std::string::npos,
                "runtime descriptor evidence must use each full frozen window hash");
    }

    Require(runtime.find("version=0.13.0") != std::string::npos &&
                runtime.find("mode=phase-7-classified-completion-manager") !=
                    std::string::npos,
            "runtime identifies the Phase 7 completion manager mode");
    Require(runtime.find("kModuleInventoryRetryCount = 20u") !=
                    std::string::npos &&
                runtime.find("kModuleInventoryRetryDelayMs = 100u") !=
                    std::string::npos &&
                runtime.find("Sleep(kModuleInventoryRetryDelayMs)") !=
                    std::string::npos &&
                runtime.find("modules = EnumerateLoadedModules()") !=
                    std::string::npos,
            "a transient empty module snapshot retries without weakening exact compatibility");
    Require(runtimeHeader.find("std::unique_ptr<CampaignMenuCapture>") !=
                    std::string::npos &&
                runtimeHeader.find(
                    "std::unique_ptr<CampaignLaunchAssociation>") !=
                    std::string::npos &&
                runtimeHeader.find("CampaignDescriptorCatalog campaignDescriptors_") !=
                    std::string::npos &&
                runtimeHeader.find("CompletionStore") !=
                    std::string::npos &&
                runtimeHeader.find("CampaignSessionAdmission") !=
                    std::string::npos &&
                runtimeHeader.find("CompletionWorker") != std::string::npos &&
                runtimeHeader.find("NativeVictoryEventSubscriber") !=
                    std::string::npos &&
                runtimeHeader.find("CampaignCompletionMarkerIndex") !=
                    std::string::npos &&
                runtimeHeader.find("CampaignMarkerObserver") !=
                    std::string::npos,
            "runtime owns the exact campaign gate, persistence, and marker state");
    Require(runtime.find("std::make_unique<CampaignMenuCapture>()") !=
                    std::string::npos &&
                runtime.find(
                    "std::make_unique<CampaignLaunchAssociation>()") !=
                    std::string::npos &&
                runtime.find("AdmitCampaignDescriptorCatalog(*executable)") !=
                    std::string::npos &&
                runtime.find("store_->Load()") !=
                    std::string::npos &&
                runtime.find("campaignMarkerIndex_.Publish(") !=
                    std::string::npos &&
                runtime.find("worker_->TryEnqueue") != std::string::npos &&
                runtime.find("nativeSubscriber_.Prepare") != std::string::npos,
            "runtime constructs persistence, native victory, and both marker indexes");

    for (const auto* forbidden : {
             "FixedMapLoadHook", "HlibCallPatchBackend", "HookSiteLayout"}) {
        Require((runtime + runtimeHeader).find(forbidden) == std::string::npos,
                "Phase 7 runtime must not own a process Hook path");
    }
    Require(runtime.find("phase-7-classified-completion-manager storage-writes=transactional") !=
                    std::string::npos &&
                runtime.find("manual-apply=revision-conflict-safe") !=
                    std::string::npos &&
                runtime.find("native-event=609-exact") !=
                    std::string::npos,
            "runtime logs the exact Phase 7 persistence boundary");

    Require(listenerHeader.find("CampaignMenuCapture& campaignCapture") !=
                    std::string::npos &&
                listenerHeader.find(
                    "CampaignLaunchAssociation& campaignAssociation") !=
                    std::string::npos &&
                listenerHeader.find("CampaignMenuCapture* campaignCapture_") !=
                    std::string::npos &&
                listenerHeader.find(
                    "CampaignLaunchAssociation* campaignAssociation_") !=
                    std::string::npos &&
                listenerHeader.find(
                    "CampaignSessionAdmission* campaignSessionAdmission_") !=
                    std::string::npos &&
                listenerHeader.find(
                    "const CampaignDescriptorCatalog* campaignDescriptors_") !=
                    std::string::npos,
            "listeners borrow but do not own campaign admission state");
    Require(listeners.find("ActiveCampaignCatalogOwner(api_)") !=
                    std::string::npos &&
                listeners.find(
                    "api->IsCurrentlyOnScreen(static_cast<S4_GUI_ENUM>(page))") !=
                    std::string::npos &&
                listeners.find("campaignCapture_->SynchronizePage(") !=
                    std::string::npos &&
                listeners.find("campaignCapture_->ObserveFrame(campaignPage, campaignPageActive)") !=
                    std::string::npos,
            "GUI callbacks prime one deterministic public owner before the first frame");
    Require(listeners.find("CopyCampaignMenuFeature(element, feature)") !=
                    std::string::npos &&
                listeners.find("IsPhase6DCampaignMissionControl(") !=
                    std::string::npos &&
                listeners.find("campaignCapture_->Invalidate()") !=
                    std::string::npos,
            "only exact static mission controls use bounded copy and malformed callbacks fail closed");
    Require(listeners.find(
                "campaignAssociation_->ObserveClick(message, element,") !=
                    std::string::npos &&
                listeners.find("*campaignDescriptors_, now") !=
                    std::string::npos &&
                listeners.find("campaignAssociation_->BeginSession(") !=
                    std::string::npos &&
                listeners.find("campaignAssociation_->ObserveIdentity(") !=
                    std::string::npos &&
                listeners.find("ValidateCampaignDescriptor(") !=
                    std::string::npos &&
                listeners.find("relative=\"") != std::string::npos,
            "click, MapInit session, confirmed identity, and immutable descriptor form the chain");
    Require(listeners.find("identity->relative") != std::string::npos &&
                listeners.find("identity->name") == std::string::npos &&
                campaignSessionAdmission.find("identity.relative") !=
                    std::string::npos &&
                campaignSessionAdmission.find("identity.name") ==
                    std::string::npos &&
                campaignDescriptors.find("identity.name") == std::string::npos &&
                campaignDescriptors.find("display") == std::string::npos,
            "runtime classification and association never use display/save name");
    Require(campaignSessionAdmission.find(
                "CampaignDescriptorValidationStatus::Matched") !=
                    std::string::npos &&
                campaignSessionAdmission.find(
                    "std::strcmp(association.descriptorKey, descriptor->key)") !=
                    std::string::npos &&
                campaignSessionAdmission.find(
                    "identity.relative != association.relative") !=
                    std::string::npos,
            "campaign admission requires exact descriptor and relative identity");
    Require(campaignDescriptors.find("association.relative !=") !=
                    std::string::npos &&
                campaignDescriptors.find("S4_SCREEN_NEWWORLD") !=
                    std::string::npos &&
                campaignDescriptors.find("CampaignDescriptorGroup::NewWorld") !=
                    std::string::npos,
            "descriptor validation compares exact same-session relative and fail-closes evidence-gap pages");
    Require(campaignCapture.find("if (!dirty_) return std::nullopt;") !=
                    std::string::npos &&
                campaignCapture.find("!IsCampaignCatalogPage(page)") !=
                    std::string::npos &&
                campaignCapture.find("EqualExceptEffects(") !=
                    std::string::npos &&
                campaignCapture.find("cached_[index].effects = feature.effects") !=
                    std::string::npos,
            "sparse page cache survives empty intervals and updates only volatile effects");
    Require(campaignAssociation.find(
                "return feature.hasText && feature.text.length != 0u;") !=
                    std::string::npos &&
                campaignAssociation.find("FindAdmittedCampaignDescriptor(") !=
                    std::string::npos &&
                campaignAssociation.find("origin.source != LaunchSource::Campaign") ==
                    std::string::npos &&
                campaignAssociation.find("ExcludedOnlineMultiplayer") !=
                    std::string::npos &&
                campaignAssociation.find(
                    "campaignPageActive && page != activePage_ && hasPending_") !=
                    std::string::npos &&
                listeners.find("campaignAssociation_->Expire(now);") !=
                    std::string::npos,
            "only labeled mission controls arm a bounded lease that expires on Tick and clears on abandoned re-entry");

    Require(listeners.find("AddMapInitListener(&OnMapInit)") !=
                    std::string::npos &&
                listeners.find("AddUIFrameListener(") != std::string::npos &&
                listeners.find("AddMouseListener(&OnMouse)") !=
                    std::string::npos &&
                listeners.find("AddGuiElementBltListener(&OnGuiElement)") !=
                    std::string::npos &&
                listeners.find("AddLuaOpenListener(&OnLuaOpen)") !=
                    std::string::npos &&
                listeners.find("AddTickListener(&OnTick)") !=
                    std::string::npos,
            "Phase 6D uses only the approved public listener surface");
    Require((runtime + runtimeHeader + listeners + listenerHeader)
                    .find("GameDefaultGameEndCheck") == std::string::npos,
            "diagnostic never invokes the behavior-producing game-end check");

    const auto coordinatorDisable = runtime.find("coordinator_->Disable()");
    const auto captureDisable = runtime.find("campaignCapture_->Disable()");
    const auto associationDisable =
        runtime.find("campaignAssociation_->Disable()");
    const auto admissionDisable =
        runtime.find("campaignSessionAdmission_->Disable()");
    const auto listenerStop = runtime.find("listeners_.Stop()", associationDisable);
    Require(coordinatorDisable != std::string::npos &&
                captureDisable != std::string::npos &&
                associationDisable != std::string::npos &&
                admissionDisable != std::string::npos &&
                listenerStop != std::string::npos &&
                coordinatorDisable < listenerStop && captureDisable < listenerStop &&
                associationDisable < listenerStop &&
                admissionDisable < listenerStop,
            "borrowed diagnostic components are disabled before listener removal");

    const auto cmake = ReadText(root / "CMakeLists.txt");
    const auto asiBegin = cmake.find("add_library(CampaignCompletionDebug");
    const auto asiEnd = cmake.find("target_include_directories", asiBegin);
    Require(asiBegin != std::string::npos && asiEnd != std::string::npos,
            "ASI source block exists");
    const auto asiSources = cmake.substr(asiBegin, asiEnd - asiBegin);
    Require(asiSources.find("src/campaign/CampaignMenuCapture.cpp") !=
                    std::string::npos &&
                asiSources.find("src/campaign/CampaignLaunchAssociation.cpp") !=
                    std::string::npos &&
                asiSources.find("src/campaign/CampaignDescriptorCatalog.cpp") !=
                    std::string::npos,
            "campaign diagnostic sources are linked into the ASI");
    for (const auto* forbiddenSource : {
             "HookSiteLayout.cpp", "FixedMapLoadHook.cpp",
             "HlibCallPatchBackend.cpp", "MsvcX86WideString.cpp"}) {
        Require(asiSources.find(forbiddenSource) == std::string::npos,
                "process Hook sources remain outside the ASI");
    }

    const auto packageScript = ReadText(root / "tools" / "package_debug.ps1");
    const auto workflow = ReadText(
        root / ".github" / "workflows" / "build-debug-asi.yml");
    Require(packageScript.find(
                "Join-Path $plugins \"CampaignCompletion\"") !=
                    std::string::npos &&
                workflow.find(
                    "Plugins/CampaignCompletion/CampaignCompletionDebug.ini") !=
                    std::string::npos,
            "package retains the plugin-relative configuration layout");
    Require(workflow.find("--config Release") != std::string::npos &&
                workflow.find("-C Release") != std::string::npos &&
                workflow.find("CampaignCompletionDebug-Win32") !=
                    std::string::npos,
            "authoritative workflow builds and tests the Release artifact");

    const auto dllMain = ReadText(root / "src" / "dllmain.cpp");
    Require(dllMain.find("RuntimeInstance().RequestControlledStop()") !=
                    std::string::npos &&
                dllMain.find("RuntimeInstance().Stop()") == std::string::npos,
            "exported stop requests control-loop shutdown only");
    Require(runtime.find(
                "static DiagnosticRuntime* const runtime = new DiagnosticRuntime();") !=
                    std::string::npos,
            "runtime has process lifetime outside loader-lock destruction");

    return 0;
}
