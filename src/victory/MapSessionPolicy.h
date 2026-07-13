#pragma once

#include "victory/LaunchOrigin.h"

#include <string_view>

namespace campaign_completion {

bool IsRandomMapIdentifier(std::wstring_view value) noexcept;
LaunchOriginSnapshot RefineLaunchOrigin(
    LaunchOriginSnapshot origin,
    std::wstring_view relativeIdentifier) noexcept;
bool ShouldRecordVictory(LaunchOriginSnapshot origin) noexcept;
bool ShouldShowCompletionMarker(LaunchOriginSnapshot origin) noexcept;

}  // namespace campaign_completion
