#include "victory/MapSessionPolicy.h"

namespace campaign_completion {
namespace {

wchar_t FoldPathUnit(wchar_t value) noexcept {
    if (value == L'/') {
        return L'\\';
    }
    if (value >= L'A' && value <= L'Z') {
        return value + (L'a' - L'A');
    }
    return value;
}

bool HasPathPrefix(std::wstring_view value,
                   std::wstring_view prefix) noexcept {
    if (value.size() <= prefix.size()) {
        return false;
    }
    for (std::size_t index = 0u; index < prefix.size(); ++index) {
        if (FoldPathUnit(value[index]) != FoldPathUnit(prefix[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool IsRandomMapIdentifier(std::wstring_view value) noexcept {
    if (value.rfind(L"RD_", 0u) == 0u) {
        return true;
    }
    return value.size() >= 2u &&
           ((value.front() == L'[' && value.back() == L']') ||
            (value.front() == L'<' && value.back() == L'>'));
}

LaunchSource CanonicalCompletionSource(
    LaunchSource admittedSource,
    std::wstring_view relativeIdentifier) noexcept {
    if (IsRandomMapIdentifier(relativeIdentifier)) {
        return LaunchSource::RandomMap;
    }
    if (admittedSource == LaunchSource::Campaign ||
        admittedSource == LaunchSource::LoadCampaign) {
        return LaunchSource::Campaign;
    }
    if (HasPathPrefix(relativeIdentifier, L"Map\\Multiplayer\\")) {
        return LaunchSource::SinglePlayerMultiplayerMap;
    }
    if (HasPathPrefix(relativeIdentifier, L"Map\\User\\")) {
        return LaunchSource::SinglePlayerCustomMap;
    }
    return LaunchSource::SinglePlayerMap;
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
        origin.eligibility = SessionEligibility::Eligible;
    }
    if (origin.eligibility == SessionEligibility::Eligible) {
        origin.source = CanonicalCompletionSource(
            origin.source, relativeIdentifier);
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
