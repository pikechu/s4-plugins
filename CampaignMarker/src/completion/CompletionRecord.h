#pragma once

#include "victory/LaunchOrigin.h"

#include <windows.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace campaign_completion {

inline constexpr std::size_t kMaximumRelativeIdentityUnits = 512u;
inline constexpr std::size_t kMaximumCompletionStringUnits = 1024u;
inline constexpr std::string_view kCompletionRecordSource =
    "native-event-609";
inline constexpr std::string_view kManualCompletionRecordSource =
    "manual-manager";
inline constexpr std::string_view kCompletionGameVersion = "2.50.1516.0";
inline constexpr std::string_view kCompletionPluginVersion = "0.7.0";

enum class CompletionMapKind {
    Fixed,
    Random,
};

struct CompletionRecord final {
    std::string stableId;
    std::string relativeId;
    std::string displayName;
    CompletionMapKind mapKind = CompletionMapKind::Fixed;
    LaunchSource launchSource = LaunchSource::Unknown;
    std::string completedAtUtc;
    std::string recordSource{ kCompletionRecordSource };
    std::string gameVersion{ kCompletionGameVersion };
    std::string pluginVersion{ kCompletionPluginVersion };
};

std::optional<std::string> WideToStrictUtf8(std::wstring_view value) noexcept;
std::optional<std::wstring> StrictUtf8ToWide(
    std::string_view value) noexcept;
std::optional<std::string> BuildStableMapId(
    std::wstring_view relative) noexcept;
CompletionMapKind CompletionKindFor(std::wstring_view relative) noexcept;
std::optional<std::string> FormatUtcCompletionTime(
    const SYSTEMTIME& utc) noexcept;
bool IsValidUtcCompletionTime(std::string_view value) noexcept;
std::string CurrentUtcCompletionTime() noexcept;

}  // namespace campaign_completion
