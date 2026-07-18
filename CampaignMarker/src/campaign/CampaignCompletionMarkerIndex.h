#pragma once

#include "campaign/CampaignDescriptorCatalog.h"
#include "completion/CompletionJson.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace campaign_completion {

enum class CampaignMarkerMatchStatus : std::uint8_t {
    None,
    Unique,
    Ambiguous,
};

struct CampaignMarkerPublishSummary final {
    std::size_t admitted = 0u;
    std::size_t rejected = 0u;
    std::size_t ambiguous = 0u;
};

class CampaignCompletionMarkerIndex final {
public:
    CampaignMarkerPublishSummary Publish(
        const CompletionDatabaseSnapshot& snapshot,
        const CampaignDescriptorCatalog& catalog) noexcept;
    CampaignMarkerMatchStatus Match(
        const CampaignDescriptorRecord& descriptor) const noexcept;

private:
    mutable std::mutex mutex_;
    std::array<CampaignMarkerMatchStatus,
               kPhase6DCampaignDescriptorCount> states_{};
};

}  // namespace campaign_completion
