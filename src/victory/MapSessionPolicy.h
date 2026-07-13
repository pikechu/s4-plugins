#pragma once

#include "victory/LaunchOrigin.h"

#include <cstdint>
#include <string_view>

namespace campaign_completion {

bool IsRandomMapIdentifier(std::wstring_view value) noexcept;
LaunchOriginSnapshot RefineLaunchOrigin(
    LaunchOriginSnapshot origin,
    std::wstring_view relativeIdentifier) noexcept;
LaunchOriginSnapshot RefineActiveSessionOrigin(
    std::uint64_t activeSessionId,
    LaunchOriginSnapshot origin,
    std::uint64_t identitySessionId,
    std::wstring_view relativeIdentifier) noexcept;
bool ShouldRecordVictory(LaunchOriginSnapshot origin) noexcept;
bool ShouldShowCompletionMarker(LaunchOriginSnapshot origin) noexcept;

}  // namespace campaign_completion
