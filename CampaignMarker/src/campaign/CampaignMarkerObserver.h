#pragma once

#include "campaign/CampaignCompletionMarkerIndex.h"
#include "campaign/CampaignMenuCapture.h"
#include "marker/FixedMapRowObserver.h"

#include <mutex>

namespace campaign_completion {

class CampaignMarkerObserver final {
public:
    CampaignMarkerObserver(
        const CampaignDescriptorCatalog& catalog,
        const CampaignCompletionMarkerIndex& index) noexcept;

    void ObservePage(DWORD owner) noexcept;
    void ObserveSnapshot(const CampaignMenuSnapshot& snapshot) noexcept;
    MarkerFrameCommands TakeFrame(DWORD callbackPage,
                                  DWORD activeOwner) noexcept;
    void Disable() noexcept;

private:
    void ClearLocked() noexcept;

    const CampaignDescriptorCatalog& catalog_;
    const CampaignCompletionMarkerIndex& index_;
    std::mutex mutex_;
    MarkerFrameCommands retained_{};
    DWORD retainedPage_ = S4_GUI_UNKNOWN;
    bool enabled_ = true;
};

}  // namespace campaign_completion
