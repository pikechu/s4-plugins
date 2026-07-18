#include "identity/MapIdentityCoordinator.h"

#include <deque>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool Contains(const std::vector<std::string>& lines,
              const std::string& fragment) {
    for (const auto& line : lines) {
        if (line.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
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
        auto value = std::move(attempts.front());
        attempts.pop_front();
        return value;
    }

    void Push(campaign_completion::LuaMapReadAttempt attempt) {
        attempts.push_back(std::move(attempt));
    }

    std::deque<campaign_completion::LuaMapReadAttempt> attempts;
    std::size_t readCount = 0u;
};

struct Harness {
    Harness()
        : coordinator(
              [this](std::string line) {
                  records.push_back(std::move(line));
              },
              [this](std::string line) {
                  trace.push_back(std::move(line));
              }) {}

    std::vector<std::string> records;
    std::vector<std::string> trace;
    campaign_completion::MapIdentityCoordinator coordinator;
};

}  // namespace

int RunMapIdentityCoordinatorTests() {
    using campaign_completion::FixedMapListKind;
    using campaign_completion::SuMapReadStatus;
    using campaign_completion::kLuaSessionDeadlineMs;

    Harness confirmed;
    QueueBridge confirmedBridge;
    confirmedBridge.Push(Success());
    confirmed.coordinator.ObserveListKind(FixedMapListKind::Single, 100u);
    confirmed.coordinator.ObserveLuaOpen(120u);
    Require(confirmed.coordinator.ObserveMapInit(200u) == 1u,
            "first coordinator MapInit gets session one");
    const auto confirmedIdentity =
        confirmed.coordinator.ObserveTick(true, 250u, confirmedBridge);
    Require(confirmedIdentity.has_value() &&
                confirmedIdentity->sessionId == 1u &&
                confirmedIdentity->name == L"Aeneas" &&
                confirmedIdentity->relative ==
                    L"Maps\\Single\\Aeneas.map",
            "confirmed identity is returned for policy refinement");
    Require(Contains(confirmed.trace, "lua-open-generation=1"),
            "Lua generation traced");
    Require(Contains(confirmed.trace, "map-init-session=1;list=single"),
            "MapInit retains explicit list epoch");
    Require(Contains(confirmed.trace, "su-map-name=Aeneas"),
            "name traced");
    Require(Contains(confirmed.trace,
                     "su-map-relative=Maps\\Single\\Aeneas.map"),
            "relative path traced");
    Require(Contains(confirmed.trace, "identity-association=confirmed"),
            "both values confirm identity");
    Require(confirmed.records == confirmed.trace,
            "record and trace sinks receive the same bounded evidence");

    Harness noList;
    QueueBridge noListBridge;
    noListBridge.Push(Success());
    noList.coordinator.ObserveLuaOpen(10u);
    noList.coordinator.ObserveMapInit(20u);
    noList.coordinator.ObserveTick(true, 70u, noListBridge);
    Require(Contains(noList.trace, "map-init-session=1;list=unknown") &&
                Contains(noList.trace, "identity-association=confirmed"),
            "campaign and load identity confirm without a fixed-list epoch");
    Require(noList.coordinator.CurrentSessionId() == 1u,
            "coordinator exposes the active map-init session");

    Harness nameOnly;
    QueueBridge nameOnlyBridge;
    nameOnlyBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Aeneas"),
                Value(SuMapReadStatus::FunctionMissing)));
    nameOnly.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    nameOnly.coordinator.ObserveLuaOpen(2u);
    nameOnly.coordinator.ObserveMapInit(100u);
    Require(!nameOnly.coordinator
                 .ObserveTick(true, 150u, nameOnlyBridge)
                 .has_value(),
            "an incomplete identity is not returned");
    Require(!nameOnly.coordinator
                 .ObserveTick(true, 100u + kLuaSessionDeadlineMs,
                              nameOnlyBridge)
                 .has_value(),
            "a name-only timeout is not returned");
    Require(Contains(nameOnly.trace, "su-map-name=Aeneas") &&
                Contains(nameOnly.trace, "su-map-relative-status=timeout") &&
                Contains(nameOnly.trace, "identity-association=name-only"),
            "name-only timeout remains diagnostic evidence");

    Harness invalid;
    QueueBridge invalidBridge;
    invalidBridge.Push(Success("Aeneas", "C:\\Maps\\Aeneas.map"));
    invalid.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    invalid.coordinator.ObserveLuaOpen(2u);
    invalid.coordinator.ObserveMapInit(3u);
    Require(!invalid.coordinator
                 .ObserveTick(true, 53u, invalidBridge)
                 .has_value(),
            "an invalid identity is not returned");
    Require(Contains(invalid.trace, "su-map-relative-status=absolute-path") &&
                Contains(invalid.trace, "identity-association=invalid-value") &&
                !Contains(invalid.trace, "su-map-relative=C:"),
            "invalid relative value is classified but never emitted");

    Harness stale;
    QueueBridge staleBridge;
    staleBridge.Push(
        Attempt(Value(SuMapReadStatus::FunctionMissing),
                Value(SuMapReadStatus::FunctionMissing)));
    stale.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    stale.coordinator.ObserveLuaOpen(2u);
    stale.coordinator.ObserveMapInit(3u);
    Require(!stale.coordinator
                 .ObserveTick(true, 53u, staleBridge)
                 .has_value(),
            "a retrying identity is not returned");
    stale.coordinator.ObserveLuaOpen(54u);
    Require(!stale.coordinator
                 .ObserveTick(true, 54u, staleBridge)
                 .has_value(),
            "a stale identity is not returned");
    Require(Contains(stale.trace, "identity-association=stale-generation") &&
                staleBridge.readCount == 1u,
            "replacement generation is terminal after Lua reads begin");

    Harness conflict;
    QueueBridge conflictBridge;
    conflictBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Aeneas"),
                Value(SuMapReadStatus::FunctionMissing)));
    conflictBridge.Push(
        Attempt(Value(SuMapReadStatus::Success, "Odysseus"),
                Value(SuMapReadStatus::FunctionMissing)));
    conflict.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    conflict.coordinator.ObserveLuaOpen(2u);
    conflict.coordinator.ObserveMapInit(100u);
    Require(!conflict.coordinator
                 .ObserveTick(true, 150u, conflictBridge)
                 .has_value(),
            "a retrying partial identity is not returned");
    Require(!conflict.coordinator
                 .ObserveTick(true, 200u, conflictBridge)
                 .has_value(),
            "a conflicting identity is not returned");
    Require(Contains(conflict.trace, "su-map-name-status=value-conflict") &&
                Contains(conflict.trace, "identity-association=conflict"),
            "changing successful value is classified as conflict");

    Harness backedOut;
    QueueBridge backedOutBridge;
    backedOutBridge.Push(Success());
    backedOut.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    backedOut.coordinator.ObserveLuaOpen(2u);
    backedOut.coordinator.ObserveMapInit(3u);
    const auto beforeBack = backedOut.trace.size();
    backedOut.coordinator.ObserveBack();
    Require(!backedOut.coordinator
                 .ObserveTick(true, 53u, backedOutBridge)
                 .has_value(),
            "a backed-out session returns no identity");
    Require(backedOut.trace.size() == beforeBack &&
                backedOutBridge.readCount == 0u,
            "Back invalidates attribution and the pending session");
    Require(backedOut.coordinator.CurrentSessionId() == 0u,
            "Back clears the active map-init session ID");
    backedOut.coordinator.ObserveListKind(FixedMapListKind::Single, 54u);
    Require(backedOut.coordinator.ObserveMapInit(55u) == 2u,
            "Back does not reset the monotonic map-init session counter");

    Harness disabled;
    QueueBridge disabledBridge;
    disabledBridge.Push(Success());
    disabled.coordinator.ObserveListKind(FixedMapListKind::Single, 1u);
    disabled.coordinator.Disable();
    disabled.coordinator.ObserveLuaOpen(2u);
    Require(disabled.coordinator.ObserveMapInit(3u) == 0u,
            "disabled coordinator rejects MapInit");
    Require(!disabled.coordinator
                 .ObserveTick(true, 53u, disabledBridge)
                 .has_value(),
            "a disabled coordinator returns no identity");
    Require(disabled.trace.empty() && disabledBridge.readCount == 0u,
            "Disable prevents records and Lua reads");
    return 0;
}
