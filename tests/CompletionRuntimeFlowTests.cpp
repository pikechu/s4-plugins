#include "completion/CompletionAdmission.h"
#include "victory/MapSessionPolicy.h"

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

campaign_completion::VictoryEventSnapshot Event(
    std::uint64_t sessionId,
    campaign_completion::NativeLocalResult result) {
    campaign_completion::VictoryEventSnapshot event{};
    event.sessionId = sessionId;
    event.fields = {campaign_completion::kNativeTerminalEventId,
                    result == campaign_completion::NativeLocalResult::Won
                        ? 1u
                        : 0u,
                    0u, 100u};
    event.result = result;
    return event;
}

campaign_completion::ConfirmedMapIdentity Fixed(std::uint64_t sessionId) {
    return {sessionId, L"Battle of the Gods",
            L"Map\\User\\Battle of the Gods.map"};
}

}  // namespace

int RunCompletionRuntimeFlowTests() {
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
            "direct and loaded runtime flows share canonical custom source");
    const LaunchOriginSnapshot online{
        LaunchSource::OnlineMultiplayer,
        SessionEligibility::ExcludedOnlineMultiplayer};

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:00:00Z"); });
        std::vector<CompletionCandidate> submitted;
        CompletionAdmission admission(
            coordinator, [&submitted](CompletionCandidate candidate) {
                submitted.push_back(std::move(candidate));
                return true;
            });
        admission.BeginSession(
            4u, {LaunchSource::LoadMapUnresolved,
                 SessionEligibility::Unknown});
        Require(!admission.ObserveIdentity(Fixed(4u), loaded),
                "identity-first flow must wait for native victory");
        Require(admission.ObserveVictory(Event(4u, NativeLocalResult::Won)),
                "matching event 609 win must submit");
        Require(submitted.size() == 1u &&
                    submitted.front().record.stableId ==
                        "map:map\\user\\battle of the gods.map" &&
                    submitted.front().record.launchSource ==
                        LaunchSource::SinglePlayerCustomMap,
                "runtime seam submits the confirmed fixed map exactly once");
        Require(!admission.ObserveVictory(Event(4u, NativeLocalResult::Won)) &&
                    submitted.size() == 1u,
                "duplicate victory must not reach the worker twice");
    }

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:01:00Z"); });
        std::vector<CompletionCandidate> submitted;
        CompletionAdmission admission(
            coordinator, [&submitted](CompletionCandidate candidate) {
                submitted.push_back(std::move(candidate));
                return true;
            });
        admission.BeginSession(5u, direct);
        Require(!admission.ObserveVictory(
                    Event(5u, NativeLocalResult::Won)),
                "victory-first flow must wait for identity");
        Require(admission.ObserveIdentity(Fixed(5u), direct) &&
                    submitted.size() == 1u &&
                    submitted.front().record.launchSource ==
                        LaunchSource::SinglePlayerCustomMap,
                "victory-first flow must submit after matching identity");
    }

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:02:00Z"); });
        std::vector<CompletionCandidate> submitted;
        CompletionAdmission admission(
            coordinator, [&submitted](CompletionCandidate candidate) {
                submitted.push_back(std::move(candidate));
                return true;
            });
        admission.BeginSession(6u, direct);
        Require(!admission.ObserveIdentity(Fixed(5u), direct) &&
                    !admission.ObserveVictory(
                        Event(6u, NativeLocalResult::Won)) &&
                    submitted.empty(),
                "identity and victory from different sessions never submit");

        admission.BeginSession(7u, direct);
        Require(!admission.ObserveIdentity(Fixed(7u), direct) &&
                    !admission.ObserveVictory(
                        Event(7u, NativeLocalResult::Lost)) &&
                    submitted.empty(),
                "local loss never submits");

        admission.BeginSession(8u, online);
        Require(!admission.ObserveIdentity(Fixed(8u), online) &&
                    !admission.ObserveVictory(
                        Event(8u, NativeLocalResult::Won)) &&
                    submitted.empty(),
                "online multiplayer victory never submits");
    }

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:03:00Z"); });
        std::vector<CompletionCandidate> submitted;
        CompletionAdmission admission(
            coordinator, [&submitted](CompletionCandidate candidate) {
                submitted.push_back(std::move(candidate));
                return true;
            });
        const LaunchOriginSnapshot random{
            LaunchSource::RandomMap, SessionEligibility::Eligible};
        admission.BeginSession(9u, random);
        Require(!admission.ObserveIdentity(
                    {9u, L"RD_LCGSDR30", L"RD_LCGSDR30"}, random) &&
                    admission.ObserveVictory(
                        Event(9u, NativeLocalResult::Won)),
                "random victory remains recordable");
        Require(submitted.size() == 1u &&
                    submitted.front().record.mapKind ==
                        CompletionMapKind::Random,
                "random record is explicitly hidden-kind data");
    }

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:04:00Z"); });
        std::vector<CompletionCandidate> submitted;
        CompletionAdmission admission(
            coordinator, [&submitted](CompletionCandidate candidate) {
                submitted.push_back(std::move(candidate));
                return true;
            });
        admission.BeginSession(10u, direct);
        Require(!admission.ObserveVictory(
                    Event(10u, NativeLocalResult::Won)),
                "pending old-session victory waits");
        admission.BeginSession(11u, direct);
        Require(!admission.ObserveIdentity(Fixed(11u), direct) &&
                    submitted.empty(),
                "new MapInit clears pending old-session state");
        admission.Disable();
        Require(!admission.ObserveVictory(
                    Event(11u, NativeLocalResult::Won)),
                "disabled admission stays inert");
    }

    {
        CompletionCandidateCoordinator coordinator(
            [] { return std::string("2026-07-14T03:05:00Z"); });
        CompletionAdmission admission(
            coordinator, [](CompletionCandidate) -> bool {
                throw std::runtime_error("submit failure");
            });
        admission.BeginSession(12u, direct);
        Require(!admission.ObserveIdentity(Fixed(12u), direct) &&
                    !admission.ObserveVictory(
                        Event(12u, NativeLocalResult::Won)),
                "submit exception must be contained on the callback thread");
    }

    return 0;
}
