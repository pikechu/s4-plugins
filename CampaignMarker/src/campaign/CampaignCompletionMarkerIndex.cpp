#include "campaign/CampaignCompletionMarkerIndex.h"

#include <utility>

namespace campaign_completion {
namespace {

std::size_t DescriptorIndex(
    const CampaignDescriptorRecord* descriptor) noexcept {
    if (descriptor == nullptr) return kPhase6DCampaignDescriptorCount;
    const auto& descriptors = AllCampaignDescriptors();
    for (std::size_t index = 0u; index < descriptors.size(); ++index) {
        if (&descriptors[index] == descriptor) return index;
    }
    return descriptors.size();
}

}  // namespace

CampaignMarkerPublishSummary CampaignCompletionMarkerIndex::Publish(
    const CompletionDatabaseSnapshot& snapshot,
    const CampaignDescriptorCatalog& catalog) noexcept {
    CampaignMarkerPublishSummary summary{};
    std::array<CampaignMarkerMatchStatus,
               kPhase6DCampaignDescriptorCount> next{};
    try {
        for (const auto& record : snapshot.records) {
            if (record.mapKind != CompletionMapKind::Fixed ||
                record.launchSource != LaunchSource::Campaign) {
                ++summary.rejected;
                continue;
            }
            const auto relative = StrictUtf8ToWide(record.relativeId);
            if (!relative.has_value() || relative->empty() ||
                relative->size() > kMaximumRelativeIdentityUnits) {
                ++summary.rejected;
                continue;
            }
            const auto stable = BuildStableMapId(*relative);
            if (!stable.has_value() || *stable != record.stableId) {
                ++summary.rejected;
                continue;
            }
            const auto* descriptor =
                FindAdmittedCampaignDescriptorRelative(catalog, *relative);
            const auto index = DescriptorIndex(descriptor);
            if (index >= next.size()) {
                ++summary.rejected;
                continue;
            }
            if (next[index] == CampaignMarkerMatchStatus::None) {
                next[index] = CampaignMarkerMatchStatus::Unique;
                ++summary.admitted;
            } else if (next[index] == CampaignMarkerMatchStatus::Unique) {
                next[index] = CampaignMarkerMatchStatus::Ambiguous;
                --summary.admitted;
                ++summary.ambiguous;
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        states_ = next;
    } catch (...) {
        next.fill(CampaignMarkerMatchStatus::None);
        std::lock_guard<std::mutex> lock(mutex_);
        states_ = next;
        return {};
    }
    return summary;
}

CampaignMarkerMatchStatus CampaignCompletionMarkerIndex::Match(
    const CampaignDescriptorRecord& descriptor) const noexcept {
    const auto index = DescriptorIndex(&descriptor);
    if (index >= states_.size()) return CampaignMarkerMatchStatus::None;
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_[index];
    } catch (...) {
        return CampaignMarkerMatchStatus::None;
    }
}

}  // namespace campaign_completion
