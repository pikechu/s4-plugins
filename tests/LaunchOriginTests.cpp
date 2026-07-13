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

    LaunchOriginTracker liveOffline;
    std::uint64_t liveNow = 100u;
    for (const DWORD page : {
             S4_SCREEN_MAINMENU,
             S4_SCREEN_SINGLEPLAYER,
             S4_SCREEN_SINGLEPLAYER_CAMPAIGN,
             S4_SCREEN_SINGLEPLAYER_MAPSELECT_SINGLE,
             S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI,
             S4_SCREEN_SINGLEPLAYER_MAPSELECT_USER,
             S4_SCREEN_SINGLEPLAYER_LOBBY,
             S4_SCREEN_MULTIPLAYER_LOBBY}) {
        liveOffline.ObservePage(page, liveNow++);
    }
    const auto liveOrigin = liveOffline.ConsumeMapInit(200u);
    Require(liveOrigin.source == LaunchSource::SinglePlayerMap &&
                liveOrigin.eligibility == SessionEligibility::Eligible,
            "live offline siblings remain eligible single player");

    LaunchOriginTracker offlineMulti;
    offlineMulti.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI, 100u);
    offlineMulti.ObserveListKind(FixedMapListKind::Multiplayer, 110u);
    const auto eligible = offlineMulti.ConsumeMapInit(200u);
    Require(eligible.source == LaunchSource::SinglePlayerMultiplayerMap,
            "offline Multiplayer Maps source retained");
    Require(eligible.eligibility == SessionEligibility::Eligible,
            "offline Multiplayer Maps remain eligible");

    LaunchOriginTracker online;
    online.ObservePage(S4_SCREEN_MULTIPLAYER, 90u);
    online.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI, 100u);
    online.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 150u);
    Require(online.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online lobby is excluded");

    LaunchOriginTracker random;
    random.ObservePage(S4_SCREEN_SINGLEPLAYER, 90u);
    random.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM, 100u);
    random.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_USER, 110u);
    Require(random.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedRandomMap,
            "random exclusion survives sibling selector callbacks");

    LaunchOriginTracker randomToFixed;
    randomToFixed.ObservePage(S4_SCREEN_SINGLEPLAYER, 90u);
    randomToFixed.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM, 100u);
    randomToFixed.ObserveListKind(FixedMapListKind::Single, 110u);
    Require(randomToFixed.ConsumeMapInit(200u).source ==
                LaunchSource::SinglePlayerMap,
            "an explicit fixed-list click can leave the random source");

    LaunchOriginTracker onlineRandom;
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER, 90u);
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM, 100u);
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 110u);
    Require(onlineRandom.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online session origin takes precedence over random map type");

    LaunchOriginTracker freshCampaign;
    freshCampaign.ObservePage(S4_SCREEN_SINGLEPLAYER, 90u);
    freshCampaign.ObservePage(S4_SCREEN_SINGLEPLAYER_CAMPAIGN, 100u);
    const auto campaign = freshCampaign.ConsumeMapInit(200u);
    Require(campaign.source == LaunchSource::Campaign &&
                campaign.eligibility == SessionEligibility::Eligible,
            "campaign remains eligible without a fixed-map lobby");

    LaunchOriginTracker campaignSibling;
    campaignSibling.ObservePage(S4_SCREEN_SINGLEPLAYER, 90u);
    campaignSibling.ObservePage(S4_SCREEN_SINGLEPLAYER_CAMPAIGN, 100u);
    campaignSibling.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_SINGLE,
                                110u);
    Require(campaignSibling.ConsumeMapInit(200u).source ==
                LaunchSource::SinglePlayerMap,
            "fixed-map lobby replaces a transient campaign sibling");

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

    LaunchOriginTracker explicitLoadedMap;
    explicitLoadedMap.ObserveLoadTypeControl(2390u, 100u);
    explicitLoadedMap.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 101u);
    explicitLoadedMap.ObservePage(S4_SCREEN_LOADGAME_MAP, 102u);
    explicitLoadedMap.ObservePage(S4_SCREEN_LOADGAME_MULTIPLAYER, 103u);
    const auto explicitMap = explicitLoadedMap.ConsumeMapInit(200u);
    Require(explicitMap.source == LaunchSource::LoadMapUnresolved &&
                explicitMap.eligibility == SessionEligibility::Unknown,
            "single-player load control survives sibling load pages");

    LaunchOriginTracker explicitLoadedCampaign;
    explicitLoadedCampaign.ObserveLoadTypeControl(2399u, 100u);
    explicitLoadedCampaign.ObservePage(S4_SCREEN_LOADGAME_MULTIPLAYER, 101u);
    Require(explicitLoadedCampaign.ConsumeMapInit(200u).source ==
                LaunchSource::LoadCampaign,
            "campaign load control survives multiplayer sibling page");

    LaunchOriginTracker explicitLoadedOnline;
    explicitLoadedOnline.ObserveLoadTypeControl(2391u, 100u);
    explicitLoadedOnline.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 101u);
    Require(explicitLoadedOnline.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "multiplayer load control survives campaign sibling page");

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
