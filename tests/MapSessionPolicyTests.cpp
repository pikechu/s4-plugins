#include "victory/MapSessionPolicy.h"

#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunMapSessionPolicyTests() {
    using namespace campaign_completion;

    Require(IsRandomMapIdentifier(L"RD_LCGSDR30"),
            "observed random key is classified");
    Require(IsRandomMapIdentifier(L"[seed]"),
            "square-bracket random form is classified");
    Require(IsRandomMapIdentifier(L"<seed>"),
            "angle-bracket random form is classified");
    Require(IsRandomMapIdentifier(L"RD_"),
            "game accepts the bare random prefix");
    Require(!IsRandomMapIdentifier(L"rd_LCGSDR30"),
            "random prefix is case-sensitive");
    Require(!IsRandomMapIdentifier(L"Map\\User\\RD_Test.map"),
            "a fixed relative path containing RD is not random");
    Require(!IsRandomMapIdentifier(L"[seed"),
            "an unclosed square form is not random");
    Require(!IsRandomMapIdentifier(L"seed>"),
            "an unopened angle form is not random");
    Require(!IsRandomMapIdentifier(L""),
            "an empty identifier is not random");

    const LaunchOriginSnapshot loaded{
        LaunchSource::LoadMapUnresolved, SessionEligibility::Unknown};
    const auto random = RefineLaunchOrigin(loaded, L"RD_LCGSDR30");
    Require(random.source == LaunchSource::RandomMap &&
                random.eligibility == SessionEligibility::Eligible,
            "a loaded random map remains recordable");
    Require(ShouldRecordVictory(random),
            "a random-map victory is recorded");
    Require(!ShouldShowCompletionMarker(random),
            "a random-map completion marker is hidden");

    const auto fixed = RefineLaunchOrigin(
        loaded, L"Map\\User\\Battle of the Gods.map");
    Require(fixed.source == LaunchSource::LoadSinglePlayerMap &&
                fixed.eligibility == SessionEligibility::Eligible,
            "a confirmed ordinary load becomes eligible");
    Require(ShouldRecordVictory(fixed),
            "an ordinary loaded-map victory is recorded");
    Require(ShouldShowCompletionMarker(fixed),
            "an ordinary loaded-map completion marker is visible");

    const LaunchOriginSnapshot online{
        LaunchSource::LoadOnlineMultiplayer,
        SessionEligibility::ExcludedOnlineMultiplayer};
    const auto onlineAfterIdentity =
        RefineLaunchOrigin(online, L"RD_LCGSDR30");
    Require(onlineAfterIdentity.source == LaunchSource::LoadOnlineMultiplayer &&
                onlineAfterIdentity.eligibility ==
                    SessionEligibility::ExcludedOnlineMultiplayer,
            "online exclusion retains precedence");
    Require(!ShouldRecordVictory(onlineAfterIdentity),
            "online multiplayer is not recorded");
    Require(!ShouldShowCompletionMarker(onlineAfterIdentity),
            "online multiplayer has no completion marker");

    const LaunchOriginSnapshot unknown{};
    const auto unknownFixed =
        RefineLaunchOrigin(unknown, L"Map\\Singleplayer\\Aeneas.map");
    Require(unknownFixed.source == LaunchSource::Unknown &&
                unknownFixed.eligibility == SessionEligibility::Unknown,
            "non-load unknown origin is not guessed eligible");
    const auto unknownRandom =
        RefineLaunchOrigin(unknown, L"RD_LCGSDR30");
    Require(unknownRandom.source == LaunchSource::RandomMap &&
                unknownRandom.eligibility == SessionEligibility::Eligible,
            "confirmed random identity resolves a missed random selector");

    const LaunchOriginSnapshot campaign{
        LaunchSource::Campaign, SessionEligibility::Eligible};
    Require(ShouldRecordVictory(campaign) &&
                ShouldShowCompletionMarker(campaign),
            "campaign victories are recordable and visible");
    return 0;
}
