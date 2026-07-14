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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

std::size_t CountOccurrences(const std::string& text,
                             const std::string& needle) {
    std::size_t count = 0u;
    for (auto position = text.find(needle); position != std::string::npos;
         position = text.find(needle, position + needle.size())) {
        ++count;
    }
    return count;
}

}  // namespace

int RunRuntimePolicyTests() {
    campaign_completion::CallbackGate gate;
    Require(gate.TryEnter(), "running gate accepts callbacks");
    gate.Leave();
    gate.CloseAndWait();
    Require(!gate.TryEnter(), "closed gate rejects callbacks");

    campaign_completion::CallbackGate blockingGate;
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
    Require(!completedWhileCallbackInFlight,
            "close must wait for the in-flight callback");
    Require(closeCompleted.load(), "close must finish after the callback leaves");

    campaign_completion::RateLimiter limiter(1000);
    Require(limiter.Allow(0), "the first event must be allowed");
    Require(!limiter.Allow(999), "an event before the interval must be rejected");
    Require(limiter.Allow(1000), "an event at the interval boundary must be allowed");

    campaign_completion::ModuleInfo approved{};
    approved.name = L"S4_Main.exe";
    approved.version = "2.50.1516.0";
    approved.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    Require(campaign_completion::CheckTargetExecutable(approved) ==
                campaign_completion::CompatibilityResult::Compatible,
            "the exact captured executable identity must be accepted");

    approved.sha256.back() = '0';
    Require(campaign_completion::CheckTargetExecutable(approved) ==
                campaign_completion::CompatibilityResult::HashMismatch,
            "a one-character executable hash mutation must fail closed");

    const auto sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto policy = ReadText(sourceRoot / "config" /
                                 "CampaignCompletionDebug.ini");
    for (const auto* required : {
             "Version=0.5.0",
             "DiagnosticMode=FixedMapRowCalibration",
             "MarkerCalibration=1",
             "NativeEventSubscription=1", "NativeTerminalEventId=609",
             "IdentitySource=SettlersUnitedLua",
             "PublicSettlementUiProbe=1", "LaunchOriginTracking=1",
             "LoadOriginRecovery=1", "InternalVictoryHook=0",
             "GameDefaultGameEndCheckCalls=0", "HookCount=0",
             "CodePatchBytes=0", "LuaWrites=0", "GameDataWrites=0",
             "CompletionDetection=1", "CompletionStorage=1",
             "CompletionMarkers=0", "CaptureTraceRoot="}) {
        Require(policy.find(required) != std::string::npos,
                "phase 5A INI policy field missing");
    }
    Require(policy.find("CaptureTraceRoot=F:") == std::string::npos,
            "packaged trace root must remain empty");
    Require(policy.find("MemoryWrites=") == std::string::npos,
            "misleading generic memory-write policy is forbidden");
    Require(policy.find("InternalHooks=") == std::string::npos,
            "obsolete public-only policy is forbidden");

    const auto runtime = ReadText(sourceRoot / "src" / "runtime" /
                                  "DiagnosticRuntime.cpp");
    const auto runtimeHeader = ReadText(sourceRoot / "src" / "runtime" /
                                        "DiagnosticRuntime.h");
    const auto packageScript = ReadText(sourceRoot / "tools" /
                                        "package_debug.ps1");
    const auto workflow = ReadText(sourceRoot / ".github" / "workflows" /
                                   "build-debug-asi.yml");
    Require((runtime + runtimeHeader).find(
                "pluginDirectory.parent_path()") == std::string::npos,
            "runtime must not derive data from the game-root parent");
    Require(packageScript.find(
                "Join-Path $plugins \"CampaignCompletion\"") !=
                std::string::npos,
            "package must stage configuration below Plugins");
    Require(workflow.find(
                "Plugins/CampaignCompletion/CampaignCompletionDebug.ini") !=
                std::string::npos,
            "workflow must require the plugin-relative INI layout");
    Require(runtime.find("version=0.5.0") != std::string::npos &&
                runtime.find("mode=fixed-map-row-calibration") !=
                    std::string::npos,
            "runtime header identifies fixed-map row calibration");
    for (const auto* forbidden : {
             "FixedMapLoadHook", "HlibCallPatchBackend", "HookSiteLayout",
             "fixedMapHook_", "hookBackend_", "originalInvoker_",
             "hookStarted_", "PatchFailure", "Start(admission"}) {
        Require((runtime + runtimeHeader).find(forbidden) ==
                    std::string::npos,
                "runtime must not own, start, or stop a process Hook");
    }
    Require(runtimeHeader.find("NativeEventAdmission") != std::string::npos &&
                runtimeHeader.find("NativeEventRegistration") !=
                    std::string::npos &&
                runtimeHeader.find("NativeVictoryEventSubscriber") !=
                    std::string::npos &&
                runtimeHeader.find("VictoryEventProbe") != std::string::npos,
            "runtime owns process-lifetime native subscription components");
    Require(runtimeHeader.find("Win32CompletionFileOps") !=
                    std::string::npos &&
                runtimeHeader.find("CompletionStore") != std::string::npos &&
                runtimeHeader.find("CompletionWorker") != std::string::npos &&
                runtimeHeader.find("CompletionCandidateCoordinator") !=
                    std::string::npos &&
                runtimeHeader.find("CompletionAdmission") !=
                    std::string::npos,
            "runtime owns completion persistence in dependency-safe storage layers");
    const auto markerTraceOwner =
        runtimeHeader.find("std::unique_ptr<MarkerCalibrationTrace>");
    const auto markerIndexOwner =
        runtimeHeader.find("std::unique_ptr<CompletionMarkerIndex>");
    const auto markerCalibrationOwner =
        runtimeHeader.find("std::unique_ptr<FixedMapRowCalibration>");
    Require(markerTraceOwner != std::string::npos &&
                markerIndexOwner != std::string::npos &&
                markerCalibrationOwner != std::string::npos &&
                markerTraceOwner < markerIndexOwner &&
                markerIndexOwner < markerCalibrationOwner,
            "runtime owns marker calibration dependencies in safe order");
    const auto storeLoad = runtime.find("const auto load = store_->Load()");
    const auto markerTraceOpen = runtime.find(
        "markerTrace_->Open(paths_->dataDirectory, GetCurrentProcessId())",
        storeLoad);
    const auto markerIndexPublish = runtime.find(
        "markerIndex_->Publish(store_->Snapshot())", storeLoad);
    Require(storeLoad != std::string::npos &&
                markerTraceOpen != std::string::npos &&
                markerIndexPublish != std::string::npos &&
                storeLoad < markerTraceOpen && storeLoad < markerIndexPublish,
            "successful store load seeds project-local marker calibration");
    Require(runtimeHeader.find("S4LuaApi") != std::string::npos &&
                runtimeHeader.find("S4LuaMapBridge") != std::string::npos &&
                runtimeHeader.find("MapIdentityCoordinator") !=
                    std::string::npos &&
                runtimeHeader.find("Phase3Trace") != std::string::npos &&
                runtimeHeader.find("LaunchOriginTracker") !=
                    std::string::npos &&
                runtimeHeader.find("SettlementUiProbe") != std::string::npos,
            "runtime owns the public calibration pipeline");
    const auto coordinatorDisable = runtime.find("coordinator_->Disable()");
    const auto originDisable = runtime.find("origin_.Disable()");
    const auto settlementDisable = runtime.find("settlement_.Disable()");
    const auto markerDisable =
        runtime.find("markerCalibration_->Disable()", settlementDisable);
    const auto listenerStop = runtime.find("listeners_.Stop()",
                                            settlementDisable);
    const auto traceClose = runtime.find("phase3Trace_.Close()", listenerStop);
    const auto markerTraceClose =
        runtime.find("markerTrace_->Close()", listenerStop);
    Require(coordinatorDisable != std::string::npos &&
                originDisable != std::string::npos &&
                settlementDisable != std::string::npos &&
                listenerStop != std::string::npos &&
                coordinatorDisable < listenerStop &&
                originDisable < listenerStop && settlementDisable < listenerStop &&
                markerDisable != std::string::npos && markerDisable < listenerStop,
            "calibration components are disabled before public listener stop");
    Require(traceClose != std::string::npos && listenerStop < traceClose,
            "phase 3 trace closes after public listeners");
    Require(markerTraceClose != std::string::npos &&
                listenerStop < markerTraceClose,
            "marker calibration trace closes after public listeners");

    const auto cmake = ReadText(sourceRoot / "CMakeLists.txt");
    const auto asiBegin = cmake.find("add_library(CampaignCompletionDebug");
    const auto asiEnd = cmake.find("target_include_directories", asiBegin);
    Require(asiBegin != std::string::npos && asiEnd != std::string::npos,
            "diagnostic ASI source block is present");
    const auto asiSources = cmake.substr(asiBegin, asiEnd - asiBegin);
    for (const auto* requiredSource : {
             "src/completion/CompletionAdmission.cpp",
             "src/completion/CompletionCandidateCoordinator.cpp",
             "src/completion/CompletionJson.cpp",
             "src/completion/CompletionRecord.cpp",
             "src/completion/CompletionStore.cpp",
             "src/completion/CompletionWorker.cpp",
             "src/completion/Win32CompletionFileOps.cpp",
             "src/diagnostics/MarkerCalibrationTrace.cpp",
             "src/marker/CompletionMarkerIndex.cpp",
             "src/marker/FixedMapRowCalibration.cpp",
             "src/native/NativeEventAdmission.cpp",
             "src/native/NativeEventRegistration.cpp",
             "src/native/NativeVictoryEventSubscriber.cpp",
             "src/victory/VictoryEventProbe.cpp"}) {
        Require(asiSources.find(requiredSource) != std::string::npos,
                "native event production source is missing from the ASI");
    }
    for (const auto* forbiddenSource : {
             "HookSiteLayout.cpp", "FixedMapLoadHook.cpp",
             "HlibCallPatchBackend.cpp", "MsvcX86WideString.cpp"}) {
        Require(asiSources.find(forbiddenSource) == std::string::npos,
                "Hook-era source must not be linked into the 0.2.5 ASI");
    }

    const auto listeners = ReadText(sourceRoot / "src" / "runtime" /
                                    "S4Listeners.cpp");
    const auto listenerHeader = ReadText(sourceRoot / "src" / "runtime" /
                                         "S4Listeners.h");
    Require(listeners.find("RefineActiveSessionOrigin") !=
                    std::string::npos &&
                listeners.find("identity->sessionId") != std::string::npos &&
                listeners.find("activeSessionId_") != std::string::npos &&
                listeners.find("origin-refinement=") != std::string::npos,
            "confirmed identity must refine only the active runtime session");
    Require(listenerHeader.find(
                "void FinishSettlementIfDue(std::uint64_t nowMs);") !=
                std::string::npos,
            "listener declares common settlement deadline operation");
    Require(listenerHeader.find("NativeVictoryEventSubscriber& subscriber") !=
                std::string::npos &&
                listenerHeader.find("VictoryEventProbe& victoryProbe") !=
                    std::string::npos,
            "listener start receives native subscriber and probe");
    Require(listenerHeader.find(
                "FixedMapRowCalibration& markerCalibration") !=
                std::string::npos &&
                listenerHeader.find(
                    "FixedMapRowCalibration* markerCalibration_") !=
                    std::string::npos,
            "listener receives but does not own marker calibration");
    Require(listenerHeader.find(
                "DispatchUiFrame(DWORD page, LPDIRECTDRAWSURFACE7 surface,") !=
                std::string::npos &&
                listenerHeader.find("INT32 pillarboxWidth)") !=
                    std::string::npos,
            "public UI frame preserves surface and pillarbox arguments");
    const auto uiBegin = listeners.find("void S4Listeners::ObserveUiFrame(");
    const auto mapBegin = listeners.find("void S4Listeners::ObserveMapInit(");
    const auto tickBegin = listeners.find("void S4Listeners::ObserveTick(");
    const auto mouseBegin = listeners.find("void S4Listeners::ObserveMouse(");
    Require(uiBegin != std::string::npos && mapBegin != std::string::npos &&
                uiBegin < mapBegin &&
                listeners.substr(uiBegin, mapBegin - uiBegin)
                        .find("FinishSettlementIfDue(now);") !=
                    std::string::npos,
            "UI frames advance the settlement deadline");
    Require(tickBegin != std::string::npos && mouseBegin != std::string::npos &&
                tickBegin < mouseBegin &&
                listeners.substr(tickBegin, mouseBegin - tickBegin)
                        .find("FinishSettlementIfDue(now);") !=
                    std::string::npos,
            "non-delayed Tick retains settlement deadline fallback");
    const auto uiSection = listeners.substr(uiBegin, mapBegin - uiBegin);
    const auto uiService = uiSection.find("ServiceNativeSubscription();");
    const auto uiLock = uiSection.find("std::lock_guard<std::mutex>");
    Require(uiService != std::string::npos && uiLock != std::string::npos &&
                uiService < uiLock,
            "UI frame services native subscription before listener mutex");
    const auto tickSection = listeners.substr(tickBegin, mouseBegin - tickBegin);
    const auto tickService = tickSection.find("ServiceNativeSubscription();");
    const auto tickLock = tickSection.find("std::lock_guard<std::mutex>");
    const auto luaOpenBegin = listeners.find("void S4Listeners::ObserveLuaOpen(");
    Require(tickService != std::string::npos && tickLock != std::string::npos &&
                tickService < tickLock,
            "non-delayed Tick services native subscription before listener mutex");
    Require(listenerHeader.find("bool nativeReinsertPending_ = false;") !=
                std::string::npos,
            "listener tracks one native list reorder per map session");
    Require(mapBegin != std::string::npos && luaOpenBegin != std::string::npos &&
                listeners.substr(mapBegin, luaOpenBegin - mapBegin)
                        .find("nativeReinsertPending_ = true;") !=
                    std::string::npos,
            "map init arms the native list reorder");
    Require(tickSection.find("nativeReinsertPending_") != std::string::npos &&
                tickSection.find("NativeSubscriptionState::Attached") !=
                    std::string::npos &&
                tickSection.find("ReinsertAtFrontOnGameThread()") !=
                    std::string::npos &&
                tickSection.find("native-subscription=reinserted-front") !=
                    std::string::npos,
            "first attached in-game Tick moves the subscriber ahead of the game GUI handler");
    Require(mapBegin != std::string::npos && luaOpenBegin != std::string::npos &&
                listeners.substr(mapBegin, luaOpenBegin - mapBegin)
                        .find("victoryProbe_->BeginSession(activeSessionId_);") !=
                    std::string::npos,
            "map init starts the native event session");
    Require(listeners.find("event-id=\" << snapshot.fields.eventId") !=
                std::string::npos &&
                listeners.find("local-result=\" << NativeResultName(") !=
                    std::string::npos &&
                listeners.find("return \"won\"") != std::string::npos &&
                listeners.find("return \"lost\"") != std::string::npos,
            "native trace uses decimal event id and neutral local result");
    Require(listeners.find("0x261") == std::string::npos &&
                listeners.find("victory-confirmed") == std::string::npos,
            "native trace contains no forbidden event or decision spelling");
    Require(CountOccurrences(listeners, "api_->GetLocalPlayer()") == 1u,
            "settlement completion reads the local player exactly once");
    Require((runtime + runtimeHeader + listeners)
                .find("GameDefaultGameEndCheck") == std::string::npos,
            "runtime must never call the behavior-producing end check");
    Require(listeners.find("AddGuiElementBltListener(&OnGuiElement)") !=
                std::string::npos,
            "public GUI-element observer remains the settlement source");
    Require(listeners.find(
                "markerCalibration_->ObservePages(snapshot.value())") !=
                std::string::npos &&
                listeners.find(
                    "markerCalibration_->ObserveListKind(listKind)") !=
                    std::string::npos &&
                listeners.find(
                    "markerCalibration_->ObserveFrame(page, pillarboxWidth)") !=
                    std::string::npos,
            "public page, tab, and frame evidence reaches marker calibration");
    Require(listeners.find(
                "CopyBoundedGuiText(element->text, copiedText)") !=
                std::string::npos &&
                listeners.find("markerCalibration_->ObserveElement(feature)") !=
                    std::string::npos,
            "GUI text is copied through the bounded public-field helper");
    auto listenersWithoutTextStyle = listeners;
    for (auto position = listenersWithoutTextStyle.find("element->textStyle");
         position != std::string::npos;
         position = listenersWithoutTextStyle.find("element->textStyle")) {
        listenersWithoutTextStyle.erase(position,
                                        std::string("element->textStyle").size());
    }
    Require(CountOccurrences(listenersWithoutTextStyle, "element->text") ==
                1u &&
                listeners.find("element->tooltipText") == std::string::npos &&
                listeners.find("element->tooltipExtraText") ==
                    std::string::npos,
            "calibration reads only the bounded map-label text pointer");
    for (const auto* forbiddenMarkerDrawing : {
             "CreateCustomUiElement", "AddGuiBltListener(",
             "IDirectDrawSurface7::GetDC", "DrawIcon", "Polyline",
             "CompletionMarkerRenderer"}) {
        Require((runtime + runtimeHeader + listeners + listenerHeader)
                    .find(forbiddenMarkerDrawing) == std::string::npos,
                "phase 5A must not link or call marker drawing");
    }
    Require(listeners.find("AddLuaOpenListener(&OnLuaOpen)") !=
                std::string::npos,
            "public LuaOpen listener must be registered");
    Require(listeners.find("AddTickListener(&OnTick)") != std::string::npos,
            "public Tick listener must be registered");
    const auto luaOpenBody = listeners.find("S4Listeners::OnLuaOpen()");
    const auto tickBody = listeners.find("S4Listeners::OnTick(");
    const auto mapInitBody = listeners.find("S4Listeners::ObserveMapInit()");
    const auto observeLuaOpenBody =
        listeners.find("S4Listeners::ObserveLuaOpen()");
    Require(luaOpenBody != std::string::npos && tickBody != std::string::npos &&
                luaOpenBody < tickBody &&
                listeners.substr(luaOpenBody, tickBody - luaOpenBody)
                        .find("bridge_") == std::string::npos,
            "LuaOpen callback performs generation bookkeeping only");
    Require(mapInitBody != std::string::npos &&
                observeLuaOpenBody != std::string::npos &&
                mapInitBody < observeLuaOpenBody &&
                listeners.substr(mapInitBody,
                                 observeLuaOpenBody - mapInitBody)
                        .find("bridge_") == std::string::npos,
            "MapInit callback never reads Lua");
    Require(listeners.find("coordinator_->ObserveTick(inGame, now, *bridge_)") !=
                std::string::npos,
            "bridge is delegated only through the bounded Tick path");

    const auto dllMain = ReadText(sourceRoot / "src" / "dllmain.cpp");
    Require(dllMain.find("RuntimeInstance().RequestControlledStop()") !=
                std::string::npos &&
                dllMain.find("RuntimeInstance().Stop()") == std::string::npos,
            "exported stop requests control-loop shutdown only");
    Require(runtime.find(
                "static DiagnosticRuntime* const runtime = new DiagnosticRuntime();") !=
                std::string::npos,
            "runtime must have process lifetime outside loader-lock destruction");
    Require(dllMain.find("DLL_PROCESS_DETACH") == std::string::npos &&
                dllMain.find("CompletionWorker") == std::string::npos,
            "DllMain must not stop or destroy persistence resources on detach");
    const auto requestDetach = runtime.find("nativeSubscriber_.RequestDetach()");
    const auto detachedCheck = runtime.find("nativeSubscriber_.Detached()",
                                             requestDetach);
    const auto controlledFailure = runtime.find(
        "controlled-stop-flush=failed", detachedCheck);
    const auto finalListenerStop = runtime.find("listeners_.Stop()",
                                                 detachedCheck);
    const auto finalTraceClose = runtime.find("phase3Trace_.Close()",
                                               finalListenerStop);
    Require(runtimeHeader.find("RequestControlledStop()") !=
                std::string::npos &&
                requestDetach != std::string::npos &&
                detachedCheck != std::string::npos &&
                requestDetach < detachedCheck,
            "controlled stop requests and confirms native detach first");
    Require(controlledFailure != std::string::npos &&
                finalListenerStop != std::string::npos &&
                controlledFailure < finalListenerStop,
            "detach timeout records failure before any public listener stop");
    Require(finalTraceClose != std::string::npos &&
                finalListenerStop < finalTraceClose,
            "successful controlled stop closes trace after public listeners");
    const auto drain = runtime.find(
        "worker_->StopAndDrain(std::chrono::milliseconds(5000))");
    const auto drainFailureReturn = runtime.find("return false;", drain);
    const auto workerReset = runtime.find("worker_.reset()", drain);
    Require(drain != std::string::npos &&
                drainFailureReturn != std::string::npos &&
                workerReset != std::string::npos &&
                drain < drainFailureReturn &&
                drainFailureReturn < workerReset,
            "drain timeout must retain worker and store ownership for later retry");
    return 0;
}
