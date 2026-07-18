#pragma once

#include "manager/CompletionManagerCatalog.h"

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace campaign_completion {

inline constexpr std::size_t kMaximumDiscoveredFixedMaps = 1024u;
inline constexpr std::size_t kMaximumDiscoveredFixedMapUnits = 262144u;

struct FixedMapDirectoryEntry final {
    std::wstring name;
    DWORD attributes = FILE_ATTRIBUTE_NORMAL;
};

enum class FixedMapDiscoveryStatus : std::uint8_t {
    Success,
    Incomplete,
    Invalid,
    IoError,
    Overflow,
};

struct FixedMapDiscoveryResult final {
    std::vector<DiscoveredFixedMap> maps;
    FixedMapDiscoveryStatus status = FixedMapDiscoveryStatus::Invalid;
    DWORD error = ERROR_SUCCESS;
};

FixedMapDiscoveryResult ValidateFixedMapDirectory(
    std::wstring relativeDirectory,
    CompletionManagerCategory category,
    const std::vector<FixedMapDirectoryEntry>& entries) noexcept;

FixedMapDiscoveryResult DiscoverFixedMaps(
    const std::filesystem::path& gameRoot) noexcept;

}  // namespace campaign_completion
