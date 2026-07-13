#pragma once

#include "S4ModApi.h"
#include "identity/ListAttribution.h"

#include <cstdint>
#include <string_view>

namespace campaign_completion {

constexpr std::uint64_t kLaunchOriginLeaseMs = 30'000u;
constexpr DWORD kLoadSinglePlayerControl = 2390u;
constexpr DWORD kLoadMultiplayerControl = 2391u;
constexpr DWORD kLoadCampaignControl = 2399u;

enum class LaunchSource {
    Unknown,
    SinglePlayerMap,
    SinglePlayerMultiplayerMap,
    SinglePlayerCustomMap,
    Campaign,
    RandomMap,
    OnlineMultiplayer,
    LoadCampaign,
    LoadSinglePlayerMap,
    LoadMapUnresolved,
    LoadOnlineMultiplayer,
};

enum class SessionEligibility {
    Unknown,
    Eligible,
    ExcludedOnlineMultiplayer,
};

struct LaunchOriginSnapshot final {
    LaunchSource source = LaunchSource::Unknown;
    SessionEligibility eligibility = SessionEligibility::Unknown;
};

std::string_view LaunchSourceName(LaunchSource value) noexcept;
std::string_view SessionEligibilityName(SessionEligibility value) noexcept;

class LaunchOriginTracker final {
public:
    void ObservePage(DWORD page, std::uint64_t nowMs) noexcept;
    void ObserveLoadTypeControl(DWORD elementId,
                                std::uint64_t nowMs) noexcept;
    void ObserveListKind(FixedMapListKind kind,
                         std::uint64_t nowMs) noexcept;
    void ObserveBack() noexcept;
    LaunchOriginSnapshot ConsumeMapInit(std::uint64_t nowMs) noexcept;
    void Disable() noexcept;

private:
    enum class NavigationContext {
        Unknown,
        SinglePlayer,
        OnlineMultiplayer,
    };

    void Set(LaunchOriginSnapshot value, std::uint64_t nowMs) noexcept;

    LaunchOriginSnapshot current_{};
    std::uint64_t observedAtMs_ = 0u;
    NavigationContext context_ = NavigationContext::Unknown;
    bool explicitLoadSelection_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
