#include "campaign/CampaignMarkerObserver.h"

#include <array>

namespace campaign_completion {
namespace {

CampaignControlIdentity IdentityFor(
    const CampaignMenuFeature& feature) noexcept {
    return {feature.valueLink, feature.x, feature.y, feature.width,
            feature.height};
}

bool ValidBounds(const CampaignMenuFeature& feature) noexcept {
    return feature.surfaceWidth != 0u && feature.surfaceHeight != 0u &&
           static_cast<DWORD>(feature.x) + feature.width <=
               feature.surfaceWidth &&
           static_cast<DWORD>(feature.y) + feature.height <=
               feature.surfaceHeight;
}

}  // namespace

CampaignMarkerObserver::CampaignMarkerObserver(
    const CampaignDescriptorCatalog& catalog,
    const CampaignCompletionMarkerIndex& index) noexcept
    : catalog_(catalog), index_(index) {}

void CampaignMarkerObserver::ObservePage(DWORD owner) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || !IsCampaignCatalogPage(owner) ||
            (retainedPage_ != S4_GUI_UNKNOWN && retainedPage_ != owner)) {
            ClearLocked();
        }
    } catch (...) {
    }
}

void CampaignMarkerObserver::ObserveSnapshot(
    const CampaignMenuSnapshot& snapshot) noexcept {
    try {
        MarkerFrameCommands next{};
        if (!enabled_ ||
            snapshot.status != CampaignMenuSnapshotStatus::Success ||
            !IsCampaignCatalogPage(snapshot.page) ||
            snapshot.count > snapshot.features.size()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ClearLocked();
            return;
        }
        std::array<const CampaignDescriptorRecord*,
                   kMaximumMarkerCommands> seen{};
        for (std::size_t index = 0u; index < snapshot.count; ++index) {
            const auto& feature = snapshot.features[index];
            const auto* descriptor = FindAdmittedCampaignDescriptor(
                catalog_, snapshot.page, IdentityFor(feature));
            if (descriptor == nullptr ||
                index_.Match(*descriptor) !=
                    CampaignMarkerMatchStatus::Unique) {
                continue;
            }
            if (!ValidBounds(feature) ||
                next.count >= next.commands.size()) {
                std::lock_guard<std::mutex> lock(mutex_);
                ClearLocked();
                return;
            }
            for (std::size_t seenIndex = 0u; seenIndex < next.count;
                 ++seenIndex) {
                if (seen[seenIndex] == descriptor) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ClearLocked();
                    return;
                }
            }
            seen[next.count] = descriptor;
            next.commands[next.count++] = {
                feature.surfaceWidth, feature.surfaceHeight, feature.x,
                feature.y, feature.width, feature.height};
        }
        next.generation = snapshot.generation;
        std::lock_guard<std::mutex> lock(mutex_);
        retained_ = next;
        retainedPage_ = snapshot.page;
    } catch (...) {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            ClearLocked();
        } catch (...) {
        }
    }
}

MarkerFrameCommands CampaignMarkerObserver::TakeFrame(
    DWORD callbackPage, DWORD activeOwner) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || retainedPage_ == S4_GUI_UNKNOWN ||
            callbackPage != retainedPage_ || activeOwner != retainedPage_) {
            return {};
        }
        return retained_;
    } catch (...) {
        return {};
    }
}

void CampaignMarkerObserver::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        ClearLocked();
    } catch (...) {
    }
}

void CampaignMarkerObserver::ClearLocked() noexcept {
    retained_ = {};
    retainedPage_ = S4_GUI_UNKNOWN;
}

}  // namespace campaign_completion
