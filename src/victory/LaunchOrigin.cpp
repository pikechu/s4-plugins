#include "victory/LaunchOrigin.h"

namespace campaign_completion {
namespace {

constexpr LaunchOriginSnapshot Origin(LaunchSource source,
                                      SessionEligibility eligibility) noexcept {
    return {source, eligibility};
}

bool IsOnline(SessionEligibility eligibility) noexcept {
    return eligibility == SessionEligibility::ExcludedOnlineMultiplayer;
}

bool IsCampaignPage(DWORD page) noexcept {
    switch (page) {
        case S4_SCREEN_TUTORIAL:
        case S4_SCREEN_ADDON:
        case S4_SCREEN_MISSIONCD:
        case S4_SCREEN_NEWWORLD:
        case S4_SCREEN_NEWWORLD2:
        case S4_SCREEN_ADDON_TROJAN:
        case S4_SCREEN_ADDON_ROMAN:
        case S4_SCREEN_ADDON_MAYAN:
        case S4_SCREEN_ADDON_VIKING:
        case S4_SCREEN_ADDON_SETTLE:
        case S4_SCREEN_MISSIONCD_ROMAN:
        case S4_SCREEN_MISSIONCD_VIKING:
        case S4_SCREEN_MISSIONCD_MAYAN:
        case S4_SCREEN_MISSIONCD_CONFL:
        case S4_SCREEN_SINGLEPLAYER_CAMPAIGN:
        case S4_SCREEN_SINGLEPLAYER_DARKTRIBE:
            return true;
        default:
            return false;
    }
}

}  // namespace

std::string_view LaunchSourceName(LaunchSource value) noexcept {
    switch (value) {
        case LaunchSource::SinglePlayerMap:
            return "single-player-map";
        case LaunchSource::SinglePlayerMultiplayerMap:
            return "single-player-multiplayer-map";
        case LaunchSource::SinglePlayerCustomMap:
            return "single-player-custom-map";
        case LaunchSource::Campaign:
            return "campaign";
        case LaunchSource::RandomMap:
            return "random-map";
        case LaunchSource::OnlineMultiplayer:
            return "online-multiplayer";
        case LaunchSource::LoadCampaign:
            return "load-campaign";
        case LaunchSource::LoadMapUnresolved:
            return "load-map-unresolved";
        case LaunchSource::LoadOnlineMultiplayer:
            return "load-online-multiplayer";
        case LaunchSource::Unknown:
        default:
            return "unknown";
    }
}

std::string_view SessionEligibilityName(SessionEligibility value) noexcept {
    switch (value) {
        case SessionEligibility::Eligible:
            return "eligible";
        case SessionEligibility::ExcludedOnlineMultiplayer:
            return "excluded-online-multiplayer";
        case SessionEligibility::ExcludedRandomMap:
            return "excluded-random-map";
        case SessionEligibility::Unknown:
        default:
            return "unknown";
    }
}

void LaunchOriginTracker::ObservePage(DWORD page,
                                      std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return;
    }

    if (page == S4_SCREEN_MAINMENU) {
        ObserveBack();
        return;
    }

    if (page == S4_SCREEN_SINGLEPLAYER) {
        context_ = NavigationContext::SinglePlayer;
        Set(Origin(LaunchSource::SinglePlayerMap,
                   SessionEligibility::Eligible),
            nowMs);
        return;
    }

    if (page == S4_SCREEN_MULTIPLAYER) {
        context_ = NavigationContext::OnlineMultiplayer;
        Set(Origin(LaunchSource::OnlineMultiplayer,
                   SessionEligibility::ExcludedOnlineMultiplayer),
            nowMs);
        return;
    }

    if (IsCampaignPage(page)) {
        context_ = NavigationContext::SinglePlayer;
        Set(Origin(LaunchSource::Campaign, SessionEligibility::Eligible),
            nowMs);
        return;
    }

    switch (page) {
        case S4_SCREEN_LOADGAME:
            ObserveBack();
            break;
        case S4_SCREEN_SINGLEPLAYER_MAPSELECT_SINGLE:
        case S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI:
        case S4_SCREEN_SINGLEPLAYER_MAPSELECT_USER:
        case S4_SCREEN_SINGLEPLAYER_LOBBY:
            if (context_ == NavigationContext::SinglePlayer) {
                Set(Origin(LaunchSource::SinglePlayerMap,
                           SessionEligibility::Eligible),
                    nowMs);
            }
            break;
        case S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM:
            if (context_ == NavigationContext::SinglePlayer) {
                Set(Origin(LaunchSource::RandomMap,
                           SessionEligibility::ExcludedRandomMap),
                    nowMs);
            }
            break;
        case S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI:
        case S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM:
        case S4_SCREEN_MULTIPLAYER_MAPSELECT_USER:
        case S4_SCREEN_MULTIPLAYER_LOBBY:
            if (context_ == NavigationContext::OnlineMultiplayer) {
                Set(Origin(LaunchSource::OnlineMultiplayer,
                           SessionEligibility::ExcludedOnlineMultiplayer),
                    nowMs);
            }
            break;
        case S4_SCREEN_LOADGAME_CAMPAIGN:
            Set(Origin(LaunchSource::LoadCampaign,
                       SessionEligibility::Eligible),
                nowMs);
            break;
        case S4_SCREEN_LOADGAME_MAP:
            Set(Origin(LaunchSource::LoadMapUnresolved,
                       SessionEligibility::Unknown),
                nowMs);
            break;
        case S4_SCREEN_LOADGAME_MULTIPLAYER:
            Set(Origin(LaunchSource::LoadOnlineMultiplayer,
                       SessionEligibility::ExcludedOnlineMultiplayer),
                nowMs);
            break;
        default:
            break;
    }
}

void LaunchOriginTracker::ObserveListKind(FixedMapListKind kind,
                                          std::uint64_t nowMs) noexcept {
    if (!enabled_ || IsOnline(current_.eligibility)) {
        return;
    }

    // A tab click is stronger evidence than the sibling selector pages that
    // Settlers IV may draw in the same frame.
    current_ = {};
    observedAtMs_ = 0u;
    context_ = NavigationContext::SinglePlayer;

    switch (kind) {
        case FixedMapListKind::Single:
            Set(Origin(LaunchSource::SinglePlayerMap,
                       SessionEligibility::Eligible),
                nowMs);
            break;
        case FixedMapListKind::Multiplayer:
            Set(Origin(LaunchSource::SinglePlayerMultiplayerMap,
                       SessionEligibility::Eligible),
                nowMs);
            break;
        case FixedMapListKind::Custom:
            Set(Origin(LaunchSource::SinglePlayerCustomMap,
                       SessionEligibility::Eligible),
                nowMs);
            break;
        case FixedMapListKind::Unknown:
        default:
            break;
    }
}

void LaunchOriginTracker::ObserveBack() noexcept {
    if (enabled_) {
        current_ = {};
        observedAtMs_ = 0u;
        context_ = NavigationContext::Unknown;
    }
}

LaunchOriginSnapshot LaunchOriginTracker::ConsumeMapInit(
    std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return {};
    }

    const auto result =
        current_.source != LaunchSource::Unknown &&
                nowMs - observedAtMs_ < kLaunchOriginLeaseMs
            ? current_
            : LaunchOriginSnapshot{};
    current_ = {};
    observedAtMs_ = 0u;
    context_ = NavigationContext::Unknown;
    return result;
}

void LaunchOriginTracker::Disable() noexcept {
    current_ = {};
    observedAtMs_ = 0u;
    context_ = NavigationContext::Unknown;
    enabled_ = false;
}

void LaunchOriginTracker::Set(LaunchOriginSnapshot value,
                              std::uint64_t nowMs) noexcept {
    if (IsOnline(current_.eligibility) &&
        !IsOnline(value.eligibility)) {
        return;
    }
    if (current_.eligibility == SessionEligibility::ExcludedRandomMap &&
        value.eligibility == SessionEligibility::Eligible) {
        return;
    }
    current_ = value;
    observedAtMs_ = nowMs;
}

}  // namespace campaign_completion
