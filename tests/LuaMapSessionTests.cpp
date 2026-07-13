#include "identity/LuaMapSession.h"

#include <deque>
#include <stdexcept>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::LuaValueResult Value(
    campaign_completion::SuMapReadStatus status,
    const char* bytes = "") {
    return {status, bytes};
}

campaign_completion::LuaMapReadAttempt Attempt(
    campaign_completion::LuaValueResult name,
    campaign_completion::LuaValueResult relative) {
    return {std::move(name), std::move(relative)};
}

campaign_completion::LuaMapReadAttempt Success(
    const char* name = "Aeneas",
    const char* relative = "Maps\\Single\\Aeneas.map") {
    using campaign_completion::SuMapReadStatus;
    return Attempt(Value(SuMapReadStatus::Success, name),
                   Value(SuMapReadStatus::Success, relative));
}

class QueueBridge final : public campaign_completion::ILuaMapBridge {
public:
    campaign_completion::LuaMapReadAttempt Read() noexcept override {
        ++readCount;
        if (attempts.empty()) {
            using campaign_completion::SuMapReadStatus;
            return Attempt(Value(SuMapReadStatus::FunctionMissing),
                           Value(SuMapReadStatus::FunctionMissing));
        }
        auto result = std::move(attempts.front());
        attempts.pop_front();
        return result;
    }

    void Push(campaign_completion::LuaMapReadAttempt attempt) {
        attempts.push_back(std::move(attempt));
    }

    std::deque<campaign_completion::LuaMapReadAttempt> attempts;
    std::size_t readCount = 0u;
};

}  // namespace

int RunLuaMapSessionTests() {
    using campaign_completion::LuaMapSession;
    using campaign_completion::SuMapReadStatus;
    using campaign_completion::kLuaMaximumAttempts;
    using campaign_completion::kLuaRetryIntervalMs;
    using campaign_completion::kLuaSessionDeadlineMs;

    Require(kLuaRetryIntervalMs == 50u, "retry interval is fixed at 50 ms");
    Require(kLuaMaximumAttempts == 64u, "attempt cap is fixed at 64");
    Require(kLuaSessionDeadlineMs == 5000u,
            "session deadline is fixed at 5000 ms");

    QueueBridge bridge;
    bridge.Push(Success());
    LuaMapSession session;
    session.ObserveLuaOpen(100u);
    const auto id = session.ObserveMapInit(200u);
    Require(id == 1u, "first map-init gets session one");
    Require(!session.ObserveTick(false, 250u, bridge).has_value(),
            "menu tick never calls Lua");
    Require(bridge.readCount == 0u, "menu tick leaves bridge untouched");
    Require(!session.ObserveTick(true, 249u, bridge).has_value(),
            "tick before interval is ignored");
    const auto success = session.ObserveTick(true, 250u, bridge);
    Require(success && success->terminal && success->name.bytes == "Aeneas" &&
                success->relative.bytes == "Maps\\Single\\Aeneas.map",
            "in-game tick resolves both values");
    Require(success->sessionId == 1u && success->luaGeneration == 1u,
            "result retains session and Lua generation");
    Require(!session.ObserveTick(true, 300u, bridge).has_value(),
            "terminal session never rereads Lua");
    Require(bridge.readCount == 1u, "terminal session reads exactly once");

    QueueBridge lateOpenBridge;
    lateOpenBridge.Push(Success());
    LuaMapSession lateOpen;
    Require(lateOpen.ObserveMapInit(10u) == 1u,
            "MapInit may precede first LuaOpen");
    Require(!lateOpen.ObserveTick(true, 60u, lateOpenBridge).has_value(),
            "unbound session cannot read Lua");
    lateOpen.ObserveLuaOpen(70u);
    const auto lateOpenResult =
        lateOpen.ObserveTick(true, 70u, lateOpenBridge);
    Require(lateOpenResult && lateOpenResult->luaGeneration == 1u,
            "first LuaOpen binds an unbound session once");

    QueueBridge staleBridge;
    LuaMapSession stale;
    stale.ObserveLuaOpen(1u);
    stale.ObserveMapInit(2u);
    stale.ObserveLuaOpen(3u);
    const auto staleResult = stale.ObserveTick(true, 52u, staleBridge);
    Require(staleResult && staleResult->terminal &&
                staleResult->name.status == SuMapReadStatus::StaleGeneration &&
                staleResult->relative.status ==
                    SuMapReadStatus::StaleGeneration,
            "second LuaOpen makes a bound session stale");
    Require(staleBridge.readCount == 0u,
            "stale generation never enters Lua");

    QueueBridge replacementBridge;
    replacementBridge.Push(Success("Odysseus", "Maps\\Single\\Odysseus.map"));
    LuaMapSession replacement;
    replacement.ObserveLuaOpen(1u);
    Require(replacement.ObserveMapInit(10u) == 1u,
            "first replacement session id is one");
    Require(replacement.ObserveMapInit(20u) == 2u,
            "second MapInit replaces and increments the session");
    const auto replacementResult =
        replacement.ObserveTick(true, 70u, replacementBridge);
    Require(replacementResult && replacementResult->sessionId == 2u &&
                replacementResult->name.bytes == "Odysseus",
            "only replacement session can complete");

    QueueBridge spacingBridge;
    spacingBridge.Push(Attempt(Value(SuMapReadStatus::FunctionMissing),
                               Value(SuMapReadStatus::FunctionMissing)));
    spacingBridge.Push(Success());
    LuaMapSession spacing;
    spacing.ObserveLuaOpen(1u);
    spacing.ObserveMapInit(100u);
    Require(!spacing.ObserveTick(true, 150u, spacingBridge).has_value(),
            "retryable attempt remains pending");
    Require(!spacing.ObserveTick(true, 199u, spacingBridge).has_value(),
            "retry is not early");
    Require(spacingBridge.readCount == 1u, "early retry does not read");
    Require(spacing.ObserveTick(true, 200u, spacingBridge).has_value(),
            "retry occurs after another 50 ms");

    QueueBridge cappedBridge;
    for (std::size_t index = 0; index < kLuaMaximumAttempts; ++index) {
        cappedBridge.Push(
            Attempt(Value(SuMapReadStatus::SuTableMissing),
                    Value(SuMapReadStatus::FunctionMissing)));
    }
    LuaMapSession capped;
    capped.ObserveLuaOpen(1u);
    capped.ObserveMapInit(100u);
    std::optional<campaign_completion::LuaMapSessionResult> cappedResult;
    for (std::size_t index = 1; index <= kLuaMaximumAttempts; ++index) {
        cappedResult = capped.ObserveTick(
            true, 100u + index * kLuaRetryIntervalMs, cappedBridge);
    }
    Require(cappedResult && cappedResult->terminal &&
                cappedResult->name.status == SuMapReadStatus::Timeout &&
                cappedBridge.readCount == kLuaMaximumAttempts,
            "64th retryable read terminates at the attempt cap");

    QueueBridge deadlineBridge;
    LuaMapSession deadline;
    deadline.ObserveLuaOpen(1u);
    deadline.ObserveMapInit(100u);
    const auto deadlineResult = deadline.ObserveTick(
        true, 100u + kLuaSessionDeadlineMs, deadlineBridge);
    Require(deadlineResult && deadlineResult->terminal &&
                deadlineResult->name.status == SuMapReadStatus::Timeout &&
                deadlineBridge.readCount == 0u,
            "deadline terminates before a late Lua read");

    QueueBridge nameOnlyBridge;
    nameOnlyBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Aeneas"),
                Value(SuMapReadStatus::FunctionMissing)));
    LuaMapSession nameOnly;
    nameOnly.ObserveLuaOpen(1u);
    nameOnly.ObserveMapInit(100u);
    Require(!nameOnly.ObserveTick(true, 150u, nameOnlyBridge).has_value(),
            "successful name is cached while relative path retries");
    const auto nameOnlyResult = nameOnly.ObserveTick(
        true, 100u + kLuaSessionDeadlineMs, nameOnlyBridge);
    Require(nameOnlyResult && nameOnlyResult->name.status ==
                                  SuMapReadStatus::Success &&
                nameOnlyResult->name.bytes == "Aeneas" &&
                nameOnlyResult->relative.status == SuMapReadStatus::Timeout,
            "deadline preserves a successful name");

    QueueBridge identicalBridge;
    identicalBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Aeneas"),
                Value(SuMapReadStatus::FunctionMissing)));
    identicalBridge.Push(Success());
    LuaMapSession identical;
    identical.ObserveLuaOpen(1u);
    identical.ObserveMapInit(100u);
    Require(!identical.ObserveTick(true, 150u, identicalBridge).has_value(),
            "partial success remains pending");
    const auto identicalResult =
        identical.ObserveTick(true, 200u, identicalBridge);
    Require(identicalResult && identicalResult->name.status ==
                                   SuMapReadStatus::Success,
            "identical repeated value remains successful");

    QueueBridge conflictBridge;
    conflictBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Aeneas"),
                Value(SuMapReadStatus::FunctionMissing)));
    conflictBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Odysseus"),
                Value(SuMapReadStatus::FunctionMissing)));
    LuaMapSession conflict;
    conflict.ObserveLuaOpen(1u);
    conflict.ObserveMapInit(100u);
    Require(!conflict.ObserveTick(true, 150u, conflictBridge).has_value(),
            "first successful value is cached");
    const auto conflictResult =
        conflict.ObserveTick(true, 200u, conflictBridge);
    Require(conflictResult && conflictResult->terminal &&
                conflictResult->name.status == SuMapReadStatus::ValueConflict,
            "different repeated success is terminal conflict");

    QueueBridge callErrorBridge;
    callErrorBridge.Push(
        Attempt(Value(SuMapReadStatus::CallError),
                Value(SuMapReadStatus::FunctionMissing)));
    LuaMapSession callError;
    callError.ObserveLuaOpen(1u);
    callError.ObserveMapInit(100u);
    const auto callErrorResult =
        callError.ObserveTick(true, 150u, callErrorBridge);
    Require(callErrorResult && callErrorResult->terminal &&
                callErrorResult->name.status == SuMapReadStatus::CallError,
            "call and type failures are immediately terminal");

    QueueBridge disabledBridge;
    disabledBridge.Push(Success());
    LuaMapSession disabled;
    disabled.ObserveLuaOpen(1u);
    disabled.ObserveMapInit(2u);
    disabled.Disable();
    disabled.ObserveLuaOpen(3u);
    Require(disabled.ObserveMapInit(4u) == 0u,
            "disabled session rejects new MapInit");
    Require(!disabled.ObserveTick(true, 100u, disabledBridge).has_value() &&
                disabledBridge.readCount == 0u,
            "Disable permanently prevents Lua reads");
    return 0;
}
