#include "victory/MapSessionPolicy.h"

namespace campaign_completion {

bool IsRandomMapIdentifier(std::wstring_view value) noexcept {
    if (value.rfind(L"RD_", 0u) == 0u) {
        return true;
    }
    return value.size() >= 2u &&
           ((value.front() == L'[' && value.back() == L']') ||
            (value.front() == L'<' && value.back() == L'>'));
}

LaunchOriginSnapshot RefineLaunchOrigin(
    LaunchOriginSnapshot origin,
    std::wstring_view relativeIdentifier) noexcept {
    if (origin.eligibility ==
        SessionEligibility::ExcludedOnlineMultiplayer) {
        return origin;
    }
    if (IsRandomMapIdentifier(relativeIdentifier)) {
        return {LaunchSource::RandomMap, SessionEligibility::Eligible};
    }
    if (origin.source == LaunchSource::LoadMapUnresolved &&
        origin.eligibility == SessionEligibility::Unknown) {
        return {LaunchSource::LoadSinglePlayerMap,
                SessionEligibility::Eligible};
    }
    return origin;
}

LaunchOriginSnapshot RefineActiveSessionOrigin(
    std::uint64_t activeSessionId,
    LaunchOriginSnapshot origin,
    std::uint64_t identitySessionId,
    std::wstring_view relativeIdentifier) noexcept {
    if (activeSessionId == 0u || identitySessionId != activeSessionId) {
        return origin;
    }
    return RefineLaunchOrigin(origin, relativeIdentifier);
}

bool ShouldRecordVictory(LaunchOriginSnapshot origin) noexcept {
    return origin.eligibility == SessionEligibility::Eligible;
}

bool ShouldShowCompletionMarker(LaunchOriginSnapshot origin) noexcept {
    return ShouldRecordVictory(origin) &&
           origin.source != LaunchSource::RandomMap;
}

}  // namespace campaign_completion
