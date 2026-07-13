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
             "Version=0.2.4", "FixedMapLoadHook=1",
             "HookSiteRva=0x000FEFA5", "HookCount=1",
             "CodePatchBytes=5", "GameDataWrites=0",
             "UnrelatedInternalCalls=0", "CompletionDetection=0",
             "CompletionStorage=0", "CompletionMarkers=0",
             "CaptureTraceRoot="}) {
        Require(policy.find(required) != std::string::npos,
                "phase 2.4 INI policy field missing");
    }
    Require(policy.find("CaptureTraceRoot=F:") == std::string::npos,
            "packaged trace root must remain empty");
    Require(policy.find("MemoryWrites=") == std::string::npos,
            "misleading generic memory-write policy is forbidden");
    Require(policy.find("InternalHooks=") == std::string::npos,
            "obsolete public-only policy is forbidden");

    const auto runtime = ReadText(sourceRoot / "src" / "runtime" /
                                  "DiagnosticRuntime.cpp");
    Require(runtime.find("version=0.2.4") != std::string::npos &&
                runtime.find("hook-mode=single-call-site") != std::string::npos,
            "runtime header identifies phase 2.4 single-call-site mode");
    const auto hookStop = runtime.find("fixedMapHook_.Stop()");
    const auto listenerStop = runtime.find("listeners_.Stop()");
    const auto traceClose = runtime.find("captureTrace_.Close()");
    Require(hookStop != std::string::npos && listenerStop != std::string::npos &&
                hookStop < listenerStop,
            "internal hook stop precedes public listener stop");
    Require(traceClose != std::string::npos && listenerStop < traceClose,
            "capture trace closes after hook and public listeners");

    const auto listeners = ReadText(sourceRoot / "src" / "runtime" /
                                    "S4Listeners.cpp");
    Require(listeners.find("AddLuaOpenListener(&OnLuaOpen)") !=
                std::string::npos,
            "public LuaOpen listener must be registered");
    Require(listeners.find("AddTickListener(&OnTick)") != std::string::npos,
            "public Tick listener must be registered");
    const auto luaOpenBody = listeners.find("S4Listeners::OnLuaOpen()");
    const auto tickBody = listeners.find("S4Listeners::OnTick(");
    const auto mapInitBody = listeners.find("S4Listeners::ObserveMapInit()");
    const auto mouseBody = listeners.find("S4Listeners::ObserveMouse(");
    Require(luaOpenBody != std::string::npos && tickBody != std::string::npos &&
                luaOpenBody < tickBody &&
                listeners.substr(luaOpenBody, tickBody - luaOpenBody)
                        .find("bridge_") == std::string::npos,
            "LuaOpen callback performs generation bookkeeping only");
    Require(mapInitBody != std::string::npos && mouseBody != std::string::npos &&
                mapInitBody < mouseBody &&
                listeners.substr(mapInitBody, mouseBody - mapInitBody)
                        .find("bridge_") == std::string::npos,
            "MapInit callback never reads Lua");
    Require(listeners.find("coordinator_->ObserveTick(inGame, now, *bridge_)") !=
                std::string::npos,
            "bridge is delegated only through the bounded Tick path");
    return 0;
}
