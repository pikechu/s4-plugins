#include "manager/FixedMapDiscovery.h"

#include "completion/CompletionRecord.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <limits>

namespace campaign_completion {
namespace {

bool FixedCategory(CompletionManagerCategory category) noexcept {
    return category == CompletionManagerCategory::FixedSinglePlayer ||
           category == CompletionManagerCategory::FixedMultiplayerList ||
           category == CompletionManagerCategory::FixedCustom;
}

bool ValidDirectory(std::wstring_view value) noexcept {
    return value == L"Map\\Singleplayer" ||
           value == L"Map\\Multiplayer" ||
           value == L"Map\\User";
}

bool ValidName(std::wstring_view value) noexcept {
    if (value.size() <= 4u || value.size() > 255u ||
        value == L"." || value == L"..") {
        return false;
    }
    for (const auto unit : value) {
        if (unit < 0x20 || unit == L'\\' || unit == L'/' ||
            unit == L':') {
            return false;
        }
    }
    const auto offset = value.size() - 4u;
    return value[offset] == L'.' &&
           std::towlower(value[offset + 1u]) == L'm' &&
           std::towlower(value[offset + 2u]) == L'a' &&
           std::towlower(value[offset + 3u]) == L'p';
}

FixedMapDiscoveryResult EnumerateOne(
    const std::filesystem::path& gameRoot,
    std::wstring_view relativeDirectory,
    CompletionManagerCategory category) {
    const auto directory = gameRoot / std::filesystem::path(relativeDirectory);
    const DWORD rootAttributes = GetFileAttributesW(directory.c_str());
    if (rootAttributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return {{}, FixedMapDiscoveryStatus::Incomplete, error};
        }
        return {{}, FixedMapDiscoveryStatus::IoError, error};
    }
    if ((rootAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u ||
        (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
        return {{}, FixedMapDiscoveryStatus::Invalid,
                ERROR_INVALID_REPARSE_DATA};
    }

    WIN32_FIND_DATAW data{};
    const auto pattern = directory / L"*";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return {{}, FixedMapDiscoveryStatus::Success, ERROR_SUCCESS};
        }
        return {{}, FixedMapDiscoveryStatus::IoError, error};
    }
    std::vector<FixedMapDirectoryEntry> entries;
    DWORD error = ERROR_SUCCESS;
    do {
        const std::wstring name(data.cFileName);
        if (name != L"." && name != L"..") {
            entries.push_back({name, data.dwFileAttributes});
            if (entries.size() > kMaximumDiscoveredFixedMaps) {
                FindClose(find);
                return {{}, FixedMapDiscoveryStatus::Overflow,
                        ERROR_BUFFER_OVERFLOW};
            }
        }
    } while (FindNextFileW(find, &data) != FALSE);
    error = GetLastError();
    FindClose(find);
    if (error != ERROR_NO_MORE_FILES) {
        return {{}, FixedMapDiscoveryStatus::IoError, error};
    }
    return ValidateFixedMapDirectory(
        std::wstring(relativeDirectory), category, entries);
}

}  // namespace

FixedMapDiscoveryResult ValidateFixedMapDirectory(
    std::wstring relativeDirectory,
    CompletionManagerCategory category,
    const std::vector<FixedMapDirectoryEntry>& entries) noexcept {
    FixedMapDiscoveryResult result{};
    try {
        if (!ValidDirectory(relativeDirectory) ||
            !FixedCategory(category)) {
            return result;
        }
        std::size_t units = 0u;
        for (const auto& entry : entries) {
            if ((entry.attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) {
                result.error = ERROR_INVALID_REPARSE_DATA;
                return result;
            }
            if ((entry.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
                if (entry.name == L"." || entry.name == L"..") continue;
                result.error = ERROR_DIRECTORY;
                return result;
            }
            if (!ValidName(entry.name)) continue;
            if (result.maps.size() >= kMaximumDiscoveredFixedMaps ||
                units > kMaximumDiscoveredFixedMapUnits -
                            relativeDirectory.size() - 1u -
                            entry.name.size()) {
                result.status = FixedMapDiscoveryStatus::Overflow;
                result.error = ERROR_BUFFER_OVERFLOW;
                result.maps.clear();
                return result;
            }
            std::wstring relative = relativeDirectory;
            relative += L'\\';
            relative += entry.name;
            if (CompletionKindFor(relative) != CompletionMapKind::Fixed ||
                !BuildStableMapId(relative).has_value()) {
                result.error = ERROR_INVALID_DATA;
                result.maps.clear();
                return result;
            }
            std::wstring display =
                entry.name.substr(0u, entry.name.size() - 4u);
            units += relative.size() + display.size();
            result.maps.push_back(
                {std::move(relative), std::move(display), category});
        }
        std::sort(result.maps.begin(), result.maps.end(),
                  [](const DiscoveredFixedMap& left,
                     const DiscoveredFixedMap& right) {
                      return left.relativeId < right.relativeId;
                  });
        result.status = FixedMapDiscoveryStatus::Success;
        result.error = ERROR_SUCCESS;
        return result;
    } catch (...) {
        result.maps.clear();
        result.status = FixedMapDiscoveryStatus::Overflow;
        result.error = ERROR_OUTOFMEMORY;
        return result;
    }
}

FixedMapDiscoveryResult DiscoverFixedMaps(
    const std::filesystem::path& gameRoot) noexcept {
    FixedMapDiscoveryResult result{};
    try {
        if (gameRoot.empty()) return result;
        constexpr std::array<std::pair<std::wstring_view,
                                      CompletionManagerCategory>, 3u>
            roots{{
                {L"Map\\Singleplayer",
                 CompletionManagerCategory::FixedSinglePlayer},
                {L"Map\\Multiplayer",
                 CompletionManagerCategory::FixedMultiplayerList},
                {L"Map\\User", CompletionManagerCategory::FixedCustom},
            }};
        bool incomplete = false;
        std::size_t units = 0u;
        for (const auto& root : roots) {
            auto one = EnumerateOne(gameRoot, root.first, root.second);
            if (one.status == FixedMapDiscoveryStatus::Incomplete) {
                incomplete = true;
                continue;
            }
            if (one.status != FixedMapDiscoveryStatus::Success) {
                return one;
            }
            if (result.maps.size() >
                    kMaximumDiscoveredFixedMaps - one.maps.size()) {
                return {{}, FixedMapDiscoveryStatus::Overflow,
                        ERROR_BUFFER_OVERFLOW};
            }
            for (const auto& map : one.maps) {
                if (units > kMaximumDiscoveredFixedMapUnits -
                                map.relativeId.size() -
                                map.displayName.size()) {
                    return {{}, FixedMapDiscoveryStatus::Overflow,
                            ERROR_BUFFER_OVERFLOW};
                }
                units += map.relativeId.size() + map.displayName.size();
                result.maps.push_back(std::move(map));
            }
        }
        result.status = incomplete ? FixedMapDiscoveryStatus::Incomplete
                                   : FixedMapDiscoveryStatus::Success;
        result.error = incomplete ? ERROR_PATH_NOT_FOUND : ERROR_SUCCESS;
        return result;
    } catch (...) {
        return {{}, FixedMapDiscoveryStatus::Overflow, ERROR_OUTOFMEMORY};
    }
}

}  // namespace campaign_completion
