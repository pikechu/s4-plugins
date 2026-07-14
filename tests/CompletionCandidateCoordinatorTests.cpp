#include "completion/CompletionCandidateCoordinator.h"
#include "victory/MapSessionPolicy.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::VictoryEventSnapshot Event(
    std::uint64_t sessionId,
    campaign_completion::NativeLocalResult result) {
    campaign_completion::VictoryEventSnapshot snapshot{};
    snapshot.sessionId = sessionId;
    snapshot.fields = {campaign_completion::kNativeTerminalEventId,
                       result == campaign_completion::NativeLocalResult::Won
                           ? 1u
                           : 0u,
                       0u, 100u};
    snapshot.result = result;
    return snapshot;
}

campaign_completion::ConfirmedMapIdentity FixedIdentity(
    std::uint64_t sessionId) {
    return {sessionId, L"Battle of the Gods",
            L"Map\\User\\Battle of the Gods.map"};
}

}  // namespace

int RunCompletionCandidateCoordinatorTests() {
    using namespace campaign_completion;

    const auto loaded = RefineLaunchOrigin(
        {LaunchSource::LoadMapUnresolved,
         SessionEligibility::Unknown},
        L"Map\\User\\Battle of the Gods.map");
    const auto direct = RefineLaunchOrigin(
        {LaunchSource::SinglePlayerMap,
         SessionEligibility::Eligible},
        L"Map\\User\\Battle of the Gods.map");
    Require(loaded.source == LaunchSource::SinglePlayerCustomMap &&
                direct.source == loaded.source,
            "direct and loaded candidates share canonical custom source");

    CompletionCandidateCoordinator victoryFirst(
        [] { return std::string("2026-07-14T01:57:38Z"); });
    victoryFirst.BeginSession(
        4u, {LaunchSource::LoadMapUnresolved, SessionEligibility::Unknown});
    Require(!victoryFirst.ObserveVictory(
                Event(4u, NativeLocalResult::Won)),
            "victory must wait for identity and refined policy");
    const auto loadedCandidate = victoryFirst.ObserveIdentity(
        FixedIdentity(4u), loaded);
    Require(loadedCandidate.has_value(),
            "matching loaded session must emit once");
    Require(loadedCandidate->sessionId == 4u,
            "candidate retains the active session");
    Require(loadedCandidate->record.stableId ==
                "map:map\\user\\battle of the gods.map",
            "candidate uses canonical stable ID");
    Require(loadedCandidate->record.relativeId ==
                "Map\\User\\Battle of the Gods.map" &&
                loadedCandidate->record.displayName == "Battle of the Gods",
            "candidate retains readable confirmed identity");
    Require(loadedCandidate->record.launchSource ==
                LaunchSource::SinglePlayerCustomMap &&
                loadedCandidate->record.completedAtUtc ==
                    "2026-07-14T01:57:38Z",
            "candidate retains canonical source and gate time");
    Require(!victoryFirst.ObserveVictory(
                Event(4u, NativeLocalResult::Won)),
            "duplicate victory must not emit twice");
    Require(!victoryFirst.ObserveIdentity(FixedIdentity(4u), loaded),
            "duplicate identity must not emit twice");

    CompletionCandidateCoordinator identityFirst(
        [] { return std::string("2026-07-14T02:00:00Z"); });
    identityFirst.BeginSession(5u, direct);
    Require(!identityFirst.ObserveIdentity(FixedIdentity(5u), direct),
            "identity must wait for victory");
    const auto directCandidate = identityFirst.ObserveVictory(
        Event(5u, NativeLocalResult::Won));
    Require(directCandidate.has_value() &&
                directCandidate->record.launchSource ==
                    LaunchSource::SinglePlayerCustomMap,
            "identity-first direct victory must emit");

    CompletionCandidateCoordinator mismatch(
        [] { return std::string("2026-07-14T02:01:00Z"); });
    mismatch.BeginSession(6u, direct);
    Require(!mismatch.ObserveIdentity(FixedIdentity(5u), direct),
            "identity from another session must be ignored");
    Require(!mismatch.ObserveVictory(Event(5u, NativeLocalResult::Won)),
            "victory from another session must be ignored");
    Require(!mismatch.ObserveVictory(Event(6u, NativeLocalResult::Won)),
            "current victory still waits for current identity");
    Require(mismatch.ObserveIdentity(FixedIdentity(6u), direct).has_value(),
            "matching observations emit after mismatches are ignored");

    CompletionCandidateCoordinator reset(
        [] { return std::string("2026-07-14T02:02:00Z"); });
    reset.BeginSession(7u, direct);
    Require(!reset.ObserveVictory(Event(7u, NativeLocalResult::Won)),
            "session 7 victory waits");
    reset.BeginSession(8u, direct);
    Require(!reset.ObserveIdentity(FixedIdentity(8u), direct),
            "new MapInit clears the previous victory");
    Require(reset.ObserveVictory(Event(8u, NativeLocalResult::Won)).has_value(),
            "new session can complete independently");

    for (const auto result : {NativeLocalResult::Lost,
                              NativeLocalResult::Malformed,
                              NativeLocalResult::Conflict,
                              NativeLocalResult::None}) {
        CompletionCandidateCoordinator rejected(
            [] { return std::string("2026-07-14T02:03:00Z"); });
        rejected.BeginSession(9u, direct);
        Require(!rejected.ObserveIdentity(FixedIdentity(9u), direct),
                "identity alone cannot complete rejected event case");
        Require(!rejected.ObserveVictory(Event(9u, result)),
                "non-winning native result must not emit");
    }

    CompletionCandidateCoordinator zero(
        [] { return std::string("2026-07-14T02:04:00Z"); });
    zero.BeginSession(0u, direct);
    Require(!zero.ObserveIdentity(FixedIdentity(0u), direct) &&
                !zero.ObserveVictory(Event(0u, NativeLocalResult::Won)),
            "session zero and orphan event must not emit");

    CompletionCandidateCoordinator unknown(
        [] { return std::string("2026-07-14T02:05:00Z"); });
    unknown.BeginSession(10u, {});
    Require(!unknown.ObserveIdentity(FixedIdentity(10u), {}) &&
                !unknown.ObserveVictory(Event(10u, NativeLocalResult::Won)),
            "unknown eligibility must not emit");

    CompletionCandidateCoordinator online(
        [] { return std::string("2026-07-14T02:06:00Z"); });
    const LaunchOriginSnapshot excluded{
        LaunchSource::OnlineMultiplayer,
        SessionEligibility::ExcludedOnlineMultiplayer};
    online.BeginSession(11u, excluded);
    Require(!online.ObserveIdentity(FixedIdentity(11u), excluded) &&
                !online.ObserveVictory(Event(11u, NativeLocalResult::Won)),
            "online multiplayer must not emit");

    CompletionCandidateCoordinator invalidIdentity(
        [] { return std::string("2026-07-14T02:07:00Z"); });
    invalidIdentity.BeginSession(12u, direct);
    Require(!invalidIdentity.ObserveIdentity(
                {12u, L"Broken", L"Map\\..\\Broken.map"}, direct) &&
                !invalidIdentity.ObserveVictory(
                    Event(12u, NativeLocalResult::Won)),
            "invalid stable identity must not emit");

    CompletionCandidateCoordinator invalidClock(
        [] { return std::string("not-a-time"); });
    invalidClock.BeginSession(13u, direct);
    Require(!invalidClock.ObserveIdentity(FixedIdentity(13u), direct) &&
                !invalidClock.ObserveVictory(
                    Event(13u, NativeLocalResult::Won)),
            "invalid gate time must not emit");

    CompletionCandidateCoordinator random(
        [] { return std::string("2026-07-14T02:08:00Z"); });
    const LaunchOriginSnapshot randomOrigin{
        LaunchSource::RandomMap, SessionEligibility::Eligible};
    random.BeginSession(14u, randomOrigin);
    Require(!random.ObserveIdentity(
                {14u, L"RD_LCGSDR30", L"RD_LCGSDR30"}, randomOrigin),
            "random identity waits for victory");
    const auto randomCandidate = random.ObserveVictory(
        Event(14u, NativeLocalResult::Won));
    Require(randomCandidate.has_value() &&
                randomCandidate->record.mapKind == CompletionMapKind::Random &&
                randomCandidate->record.stableId == "map:rd_lcgsdr30",
            "recordable random victory emits a hidden-kind record");

    CompletionCandidateCoordinator disabled(
        [] { return std::string("2026-07-14T02:09:00Z"); });
    disabled.BeginSession(15u, direct);
    disabled.Disable();
    Require(!disabled.ObserveIdentity(FixedIdentity(15u), direct) &&
                !disabled.ObserveVictory(
                    Event(15u, NativeLocalResult::Won)),
            "disabled coordinator must stay inert");
    return 0;
}
