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
             "Version=0.3.1",
             "DiagnosticMode=NativeVictoryEventCalibration",
             "NativeEventSubscription=1", "NativeTerminalEventId=609",
             "IdentitySource=SettlersUnitedLua",
             "PublicSettlementUiProbe=1", "LaunchOriginTracking=1",
             "LoadOriginRecovery=1", "InternalVictoryHook=0",
             "GameDefaultGameEndCheckCalls=0", "HookCount=0",
             "CodePatchBytes=0", "LuaWrites=0", "GameDataWrites=0",
             "CompletionDetection=0", "CompletionStorage=0",
             "CompletionMarkers=0", "CaptureTraceRoot="}) {
        Require(policy.find(required) != std::string::npos,
                "phase 3B INI policy field missing");
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
    Require(runtime.find("version=0.3.1") != std::string::npos &&
                runtime.find("mode=native-event-calibration") !=
                    std::string::npos,
            "runtime header identifies phase 3B native event calibration");
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
    const auto listenerStop = runtime.find("listeners_.Stop()",
                                            settlementDisable);
    const auto traceClose = runtime.find("phase3Trace_.Close()", listenerStop);
    Require(coordinatorDisable != std::string::npos &&
                originDisable != std::string::npos &&
                settlementDisable != std::string::npos &&
                listenerStop != std::string::npos &&
                coordinatorDisable < listenerStop &&
                originDisable < listenerStop && settlementDisable < listenerStop,
            "calibration components are disabled before public listener stop");
    Require(traceClose != std::string::npos && listenerStop < traceClose,
            "phase 3 trace closes after public listeners");

    const auto cmake = ReadText(sourceRoot / "CMakeLists.txt");
    const auto asiBegin = cmake.find("add_library(CampaignCompletionDebug");
    const auto asiEnd = cmake.find("target_include_directories", asiBegin);
    Require(asiBegin != std::string::npos && asiEnd != std::string::npos,
            "diagnostic ASI source block is present");
    const auto asiSources = cmake.substr(asiBegin, asiEnd - asiBegin);
    for (const auto* requiredSource : {
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
    Require(listenerHeader.find(
                "void FinishSettlementIfDue(std::uint64_t nowMs);") !=
                std::string::npos,
            "listener declares common settlement deadline operation");
    Require(listenerHeader.find("NativeVictoryEventSubscriber& subscriber") !=
                std::string::npos &&
                listenerHeader.find("VictoryEventProbe& victoryProbe") !=
                    std::string::npos,
            "listener start receives native subscriber and probe");
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
    Require(tickService != std::string::npos && tickLock != std::string::npos &&
                tickService < tickLock,
            "non-delayed Tick services native subscription before listener mutex");
    const auto luaOpenBegin = listeners.find("void S4Listeners::ObserveLuaOpen(");
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
    auto listenersWithoutTextStyle = listeners;
    for (auto position = listenersWithoutTextStyle.find("element->textStyle");
         position != std::string::npos;
         position = listenersWithoutTextStyle.find("element->textStyle")) {
        listenersWithoutTextStyle.erase(position,
                                        std::string("element->textStyle").size());
    }
    Require(listenersWithoutTextStyle.find("element->text") ==
                std::string::npos &&
                listeners.find("element->tooltipText") == std::string::npos &&
                listeners.find("element->tooltipExtraText") ==
                    std::string::npos,
            "calibration must not dereference GUI text pointers");
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
    return 0;
}
