#pragma once

#include "campaign/CampaignMenuCapture.h"
#include "identity/MapIdentityCoordinator.h"
#include "victory/LaunchOrigin.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace campaign_completion {

struct CampaignDescriptorCatalog;

inline constexpr std::uint64_t kCampaignLaunchAssociationLeaseMs = 30'000u;

struct CampaignControlIdentity final {
    WORD controlId = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
};

struct CampaignMenuAssociation final {
    DWORD page = S4_GUI_UNKNOWN;
    CampaignControlIdentity control{};
    std::uint64_t sessionId = 0u;
    const char* descriptorKey = nullptr;
    std::wstring relative;
};

class CampaignLaunchAssociation final {
public:
    void ObservePage(DWORD page) noexcept;
    void ObserveSnapshot(const CampaignMenuSnapshot& snapshot) noexcept;
    bool ObserveClick(DWORD message, const S4UiElement* element,
                      const CampaignDescriptorCatalog& catalog,
                      std::uint64_t nowMs) noexcept;
    bool BeginSession(std::uint64_t sessionId, LaunchOriginSnapshot origin,
                      std::uint64_t nowMs) noexcept;
    std::optional<CampaignMenuAssociation> ObserveIdentity(
        const ConfirmedMapIdentity& identity,
        std::uint64_t nowMs) noexcept;
    void Expire(std::uint64_t nowMs) noexcept;
    void Disable() noexcept;
    std::string_view PendingDescriptorKey() const noexcept;

private:
    void ClearPending() noexcept;

    CampaignMenuSnapshot snapshot_{};
    CampaignControlIdentity pendingControl_{};
    const char* pendingDescriptorKey_ = nullptr;
    std::uint64_t clickedAtMs_ = 0u;
    std::uint64_t sessionId_ = 0u;
    DWORD activePage_ = S4_GUI_UNKNOWN;
    DWORD pendingPage_ = S4_GUI_UNKNOWN;
    bool pageActive_ = false;
    bool hasSnapshot_ = false;
    bool hasPending_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
