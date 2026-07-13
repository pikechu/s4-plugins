#include "victory/LaunchOrigin.h"

#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunLaunchOriginTests() {
    using namespace campaign_completion;

    LaunchOriginTracker offlineMulti;
    offlineMulti.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI, 100u);
    offlineMulti.ObserveListKind(FixedMapListKind::Multiplayer, 110u);
    const auto eligible = offlineMulti.ConsumeMapInit(200u);
    Require(eligible.source == LaunchSource::SinglePlayerMultiplayerMap,
            "offline Multiplayer Maps source retained");
    Require(eligible.eligibility == SessionEligibility::Eligible,
            "offline Multiplayer Maps remain eligible");

    LaunchOriginTracker online;
    online.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI, 100u);
    online.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 150u);
    Require(online.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online lobby is excluded");

    LaunchOriginTracker random;
    random.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM, 100u);
    Require(random.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedRandomMap,
            "single-player random map is excluded");

    LaunchOriginTracker onlineRandom;
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM, 100u);
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 110u);
    Require(onlineRandom.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online session origin takes precedence over random map type");

    LaunchOriginTracker loadedMap;
    loadedMap.ObservePage(S4_SCREEN_LOADGAME_MAP, 100u);
    const auto unresolved = loadedMap.ConsumeMapInit(200u);
    Require(unresolved.source == LaunchSource::LoadMapUnresolved &&
                unresolved.eligibility == SessionEligibility::Unknown,
            "ordinary loaded map waits for random-source evidence");

    LaunchOriginTracker loadedCampaign;
    loadedCampaign.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 100u);
    Require(loadedCampaign.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::Eligible,
            "campaign load is eligible");

    LaunchOriginTracker loadedOnline;
    loadedOnline.ObservePage(S4_SCREEN_LOADGAME_MULTIPLAYER, 100u);
    Require(loadedOnline.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "multiplayer load is excluded");

    LaunchOriginTracker expired;
    expired.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 1u);
    Require(expired.ConsumeMapInit(1u + kLaunchOriginLeaseMs).source ==
                LaunchSource::Unknown,
            "expired origin fails closed");
    Require(expired.ConsumeMapInit(2u + kLaunchOriginLeaseMs).source ==
                LaunchSource::Unknown,
            "origin is consumed once");

    LaunchOriginTracker disabled;
    disabled.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 1u);
    disabled.Disable();
    disabled.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM, 2u);
    Require(disabled.ConsumeMapInit(3u).source == LaunchSource::Unknown,
            "disabled tracker remains inert");
    return 0;
}
