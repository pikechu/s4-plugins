#include "marker/CompletionMarkerIndex.h"

#include <windows.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace campaign_completion {
namespace {

constexpr std::size_t kMaximumMarkerDisplayNameUnits = 256u;

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

bool EqualOrdinalInsensitive(std::wstring_view left,
                             std::wstring_view right) noexcept {
    if (left.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        right.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    return CompareStringOrdinal(
               left.data(), static_cast<int>(left.size()), right.data(),
               static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool HasMapExtension(std::wstring_view value) noexcept {
    return value.size() > 4u &&
           EqualOrdinalInsensitive(value.substr(value.size() - 4u), L".map");
}

std::optional<std::wstring_view> FilenameStem(
    std::wstring_view relative) noexcept {
    if (!HasMapExtension(relative)) {
        return std::nullopt;
    }
    const auto separator = relative.find_last_of(L"\\/");
    const std::size_t start =
        separator == std::wstring_view::npos ? 0u : separator + 1u;
    if (start >= relative.size() - 4u) {
        return std::nullopt;
    }
    return relative.substr(start, relative.size() - start - 4u);
}

bool HasControlCharacter(std::wstring_view value) noexcept {
    return std::any_of(value.begin(), value.end(), [](wchar_t unit) {
        const auto code = static_cast<unsigned>(unit);
        return code <= 0x1Fu || (code >= 0x7Fu && code <= 0x9Fu);
    });
}

bool SourceMatches(FixedMapListKind kind, LaunchSource source) noexcept {
    switch (kind) {
        case FixedMapListKind::Single:
            return source == LaunchSource::SinglePlayerMap;
        case FixedMapListKind::Multiplayer:
            return source == LaunchSource::SinglePlayerMultiplayerMap;
        case FixedMapListKind::Custom:
            return source == LaunchSource::SinglePlayerCustomMap;
        case FixedMapListKind::Unknown:
        default:
            return false;
    }
}

}  // namespace

FixedMapListKind FixedListKindForRecord(
    const CompletionRecord& record) noexcept {
    try {
        if (record.mapKind != CompletionMapKind::Fixed ||
            record.launchSource == LaunchSource::OnlineMultiplayer ||
            record.launchSource == LaunchSource::LoadOnlineMultiplayer) {
            return FixedMapListKind::Unknown;
        }
        const auto relative = StrictUtf8ToWide(record.relativeId);
        if (!relative.has_value()) {
            return FixedMapListKind::Unknown;
        }

        FixedMapListKind kind = FixedMapListKind::Unknown;
        if (HasPathPrefix(*relative, L"Map\\Singleplayer\\")) {
            kind = FixedMapListKind::Single;
        } else if (HasPathPrefix(*relative, L"Map\\Multiplayer\\")) {
            kind = FixedMapListKind::Multiplayer;
        } else if (HasPathPrefix(*relative, L"Map\\User\\")) {
            kind = FixedMapListKind::Custom;
        }
        return SourceMatches(kind, record.launchSource)
                   ? kind
                   : FixedMapListKind::Unknown;
    } catch (...) {
        return FixedMapListKind::Unknown;
    }
}

void CompletionMarkerIndex::Publish(
    const CompletionDatabaseSnapshot& snapshot) noexcept {
    try {
        MarkerCandidateSnapshot candidate;
        candidate.reserve(snapshot.records.size());
        for (const auto& record : snapshot.records) {
            const auto listKind = FixedListKindForRecord(record);
            if (listKind == FixedMapListKind::Unknown) {
                continue;
            }
            const auto relative = StrictUtf8ToWide(record.relativeId);
            const auto displayName = StrictUtf8ToWide(record.displayName);
            if (!relative.has_value() || !displayName.has_value() ||
                displayName->empty() ||
                displayName->size() > kMaximumMarkerDisplayNameUnits ||
                HasControlCharacter(*displayName)) {
                continue;
            }
            const auto stem = FilenameStem(*relative);
            if (!stem.has_value() ||
                !EqualOrdinalInsensitive(*stem, *displayName)) {
                continue;
            }
            candidate.push_back(
                {record.stableId, std::move(*displayName), listKind});
        }

        std::lock_guard<std::mutex> lock(mutex_);
        candidates_.swap(candidate);
    } catch (...) {
    }
}

MarkerCandidateSnapshot CompletionMarkerIndex::Find(
    FixedMapListKind listKind,
    std::wstring_view rowLabel) const noexcept {
    if (listKind == FixedMapListKind::Unknown || rowLabel.empty()) {
        return {};
    }
    try {
        MarkerCandidateSnapshot matches;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& candidate : candidates_) {
            if (candidate.listKind == listKind &&
                EqualOrdinalInsensitive(candidate.displayName, rowLabel)) {
                matches.push_back(candidate);
            }
        }
        return matches;
    } catch (...) {
        return {};
    }
}

}  // namespace campaign_completion
