#pragma once

#include "completion/CompletionJson.h"
#include "identity/ListAttribution.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace campaign_completion {

struct MarkerCandidate final {
    std::string stableId;
    std::wstring displayName;
    FixedMapListKind listKind = FixedMapListKind::Unknown;
};

using MarkerCandidateSnapshot = std::vector<MarkerCandidate>;

FixedMapListKind FixedListKindForRecord(
    const CompletionRecord& record) noexcept;

class CompletionMarkerIndex final {
public:
    void Publish(const CompletionDatabaseSnapshot& snapshot) noexcept;
    MarkerCandidateSnapshot Find(
        FixedMapListKind listKind,
        std::wstring_view rowLabel) const noexcept;

private:
    mutable std::mutex mutex_;
    MarkerCandidateSnapshot candidates_;
};

}  // namespace campaign_completion
