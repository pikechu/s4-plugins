#include "campaign/CampaignSessionAdmission.h"

#include <cstring>
#include <string_view>

namespace campaign_completion {
namespace {

bool ExplicitlyExcluded(LaunchOriginSnapshot origin) noexcept {
    return origin.eligibility ==
               SessionEligibility::ExcludedOnlineMultiplayer ||
           origin.source == LaunchSource::OnlineMultiplayer ||
           origin.source == LaunchSource::LoadOnlineMultiplayer;
}

}  // namespace

void CampaignSessionAdmission::BeginSession(
    std::uint64_t sessionId, LaunchOriginSnapshot origin) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        sessionId_ = sessionId;
        origin_ = origin;
        consumed_ = false;
    } catch (...) {
    }
}

std::optional<LaunchOriginSnapshot> CampaignSessionAdmission::Observe(
    const ConfirmedMapIdentity& identity,
    const CampaignMenuAssociation& association,
    const CampaignDescriptorValidation& validation) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || consumed_) return std::nullopt;
        consumed_ = true;
        const auto* descriptor = validation.record;
        if (sessionId_ == 0u || identity.sessionId != sessionId_ ||
            association.sessionId != sessionId_ ||
            ExplicitlyExcluded(origin_) ||
            validation.status !=
                CampaignDescriptorValidationStatus::Matched ||
            descriptor == nullptr || descriptor->key == nullptr ||
            descriptor->relative == nullptr ||
            association.descriptorKey == nullptr ||
            std::strcmp(association.descriptorKey, descriptor->key) != 0 ||
            association.relative !=
                std::wstring_view(descriptor->relative) ||
            identity.relative != association.relative) {
            return std::nullopt;
        }
        return LaunchOriginSnapshot{
            LaunchSource::Campaign, SessionEligibility::Eligible};
    } catch (...) {
        return std::nullopt;
    }
}

void CampaignSessionAdmission::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        origin_ = {};
        sessionId_ = 0u;
        consumed_ = true;
    } catch (...) {
    }
}

}  // namespace campaign_completion
