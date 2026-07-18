#pragma once

#include "completion/CompletionJson.h"

#include <cstdint>
#include <string>
#include <vector>

namespace campaign_completion {

inline constexpr std::size_t kMaximumManualCompletionEntries = 2048u;

struct ManualCompletionEntry final {
    std::string stableId;
    CompletionRecord record;
    bool desiredCompleted = false;
    bool editable = false;
};

struct ManualCompletionRequest final {
    std::uint64_t baselineRevision = 0u;
    std::vector<ManualCompletionEntry> entries;
};

enum class ManualCompletionStatus : std::uint8_t {
    Changed,
    Unchanged,
    Conflict,
    Invalid,
};

struct ManualCompletionCandidate final {
    ManualCompletionStatus status = ManualCompletionStatus::Invalid;
    CompletionDatabaseSnapshot snapshot;
};

ManualCompletionCandidate BuildManualCompletionCandidate(
    const CompletionDatabaseSnapshot& baseline,
    std::uint64_t currentRevision,
    const ManualCompletionRequest& request) noexcept;

}  // namespace campaign_completion
