#pragma once

#include "campaign/CampaignDescriptorCatalog.h"
#include "identity/MapIdentityCoordinator.h"
#include "victory/LaunchOrigin.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace campaign_completion {

class CampaignSessionAdmission final {
public:
    void BeginSession(std::uint64_t sessionId,
                      LaunchOriginSnapshot origin) noexcept;
    std::optional<LaunchOriginSnapshot> Observe(
        const ConfirmedMapIdentity& identity,
        const CampaignMenuAssociation& association,
        const CampaignDescriptorValidation& validation) noexcept;
    void Disable() noexcept;

private:
    mutable std::mutex mutex_;
    LaunchOriginSnapshot origin_{};
    std::uint64_t sessionId_ = 0u;
    bool consumed_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
