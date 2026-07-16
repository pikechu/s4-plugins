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
             "Version=0.9.0",
             "DiagnosticMode=ImmutableCampaignDescriptorDiagnostic",
             "InternalMenuReadOnly=0", "InternalMenuWrites=0",
             "InternalMenuRendering=0", "PublicMarkerFallback=0",
             "PublicSettlementUiProbe=0", "LaunchOriginTracking=1",
             "LoadOriginRecovery=0", "NativeEventSubscription=0",
             "InternalVictoryHook=0", "HookCount=0", "CodePatchBytes=0",
             "GameDefaultGameEndCheckCalls=0", "LuaWrites=0",
             "GameDataWrites=0", "CompletionDetection=0",
             "CompletionStorage=0", "CompletionMarkers=0",
             "CampaignMenuPublicCapture=1",
             "CampaignMenuPages=3,4,5,6,11,12,13,14,15,16,17,18,19,20,21",
             "CampaignMenuAssociation=SettlersUnitedLua",
             "CampaignMenuCache=PageResidencySparse",
             "CampaignLaunchLeaseMs=30000",
             "CampaignDescriptorCatalog=ImmutableStaticReadOnly",
             "CampaignDescriptorIdentity=SameSessionRelativeExact",
             "CampaignDescriptorGroups=AddOn,MissionCD,Original,DarkTribe",
             "CampaignDescriptorEvidenceGaps=NewWorld,NewWorld2",
             "IdentitySource=SettlersUnitedLua", "CaptureTraceRoot="}) {
        Require(policy.find(required) != std::string::npos,
                "Phase 6C packaged policy field is missing");
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

    Require(runtime.find("version=0.9.0") != std::string::npos &&
                runtime.find("mode=immutable-campaign-descriptor-diagnostic") !=
                    std::string::npos,
            "runtime identifies the Phase 6C diagnostic mode");
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
                    std::string::npos,
            "runtime owns bounded capture, association, and immutable descriptors");
    Require(runtime.find("std::make_unique<CampaignMenuCapture>()") !=
                    std::string::npos &&
                runtime.find(
                    "std::make_unique<CampaignLaunchAssociation>()") !=
                    std::string::npos &&
                runtime.find("AdmitCampaignDescriptorCatalog(*executable)") !=
                    std::string::npos,
            "runtime constructs the read-only diagnostic components");

    for (const auto* forbidden : {
             "Win32CompletionFileOps", "CompletionStore",
             "CompletionWorker", "CompletionAdmission",
             "CompletionCandidateCoordinator", "NativeEventAdmission::Create",
             "NativeEventRegistration", "NativeVictoryEventSubscriber",
             "VictoryEventProbe", "CompletionMarkerIndex",
             "FixedMapRowObserver", "DirectDrawMarkerSurface",
             "CompletionMarkerRenderer", "AdmitFixedMapMenuMemory",
             "FixedMapLoadHook", "HlibCallPatchBackend", "HookSiteLayout"}) {
        Require((runtime + runtimeHeader).find(forbidden) == std::string::npos,
                "Phase 6C runtime must not own a writer, native event, marker, or Hook path");
    }
    Require(runtime.find("phase-6c-read-only storage=disabled") !=
                    std::string::npos,
            "runtime logs the enforced read-only construction boundary");

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
                    "const CampaignDescriptorCatalog* campaignDescriptors_") !=
                    std::string::npos,
            "listeners borrow but do not own campaign diagnostic state");
    Require(listeners.find("ActiveCampaignCatalogOwner(api_)") !=
                    std::string::npos &&
                listeners.find(
                    "api->IsCurrentlyOnScreen(static_cast<S4_GUI_ENUM>(page))") !=
                    std::string::npos &&
                listeners.find("campaignCapture_->ObserveFrame(campaignPage, campaignPageActive)") !=
                    std::string::npos,
            "one deterministic public owner retains composed campaign layers");
    Require(listeners.find("CopyCampaignMenuFeature(element, feature)") !=
                    std::string::npos &&
                listeners.find("campaignCapture_->Invalidate()") !=
                    std::string::npos,
            "GUI values use bounded copy and malformed callbacks fail closed");
    Require(listeners.find(
                "campaignAssociation_->ObserveClick(message, element, now)") !=
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
                campaignDescriptors.find("identity.name") == std::string::npos &&
                campaignDescriptors.find("display") == std::string::npos,
            "runtime classification and association never use display/save name");
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
            "Phase 6C uses only the approved public listener surface");
    Require((runtime + runtimeHeader + listeners + listenerHeader)
                    .find("GameDefaultGameEndCheck") == std::string::npos,
            "diagnostic never invokes the behavior-producing game-end check");

    const auto coordinatorDisable = runtime.find("coordinator_->Disable()");
    const auto captureDisable = runtime.find("campaignCapture_->Disable()");
    const auto associationDisable =
        runtime.find("campaignAssociation_->Disable()");
    const auto listenerStop = runtime.find("listeners_.Stop()", associationDisable);
    Require(coordinatorDisable != std::string::npos &&
                captureDisable != std::string::npos &&
                associationDisable != std::string::npos &&
                listenerStop != std::string::npos &&
                coordinatorDisable < listenerStop && captureDisable < listenerStop &&
                associationDisable < listenerStop,
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
