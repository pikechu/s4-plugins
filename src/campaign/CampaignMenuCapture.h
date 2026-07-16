#pragma once

#include "S4ModApi.h"
#include "marker/BoundedMenuText.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace campaign_completion {

inline constexpr S4_GUI_ENUM kDarkTribeCampaignPage =
    S4_SCREEN_SINGLEPLAYER_DARKTRIBE;
inline constexpr std::size_t kMaximumCampaignMenuFeatures = 128u;
inline constexpr std::array<DWORD, 15u> kCampaignCatalogPages{
    S4_SCREEN_ADDON,
    S4_SCREEN_MISSIONCD,
    S4_SCREEN_NEWWORLD,
    S4_SCREEN_NEWWORLD2,
    S4_SCREEN_ADDON_TROJAN,
    S4_SCREEN_ADDON_ROMAN,
    S4_SCREEN_ADDON_MAYAN,
    S4_SCREEN_ADDON_VIKING,
    S4_SCREEN_ADDON_SETTLE,
    S4_SCREEN_MISSIONCD_ROMAN,
    S4_SCREEN_MISSIONCD_VIKING,
    S4_SCREEN_MISSIONCD_MAYAN,
    S4_SCREEN_MISSIONCD_CONFL,
    S4_SCREEN_SINGLEPLAYER_CAMPAIGN,
    S4_SCREEN_SINGLEPLAYER_DARKTRIBE,
};

bool IsCampaignCatalogPage(DWORD page) noexcept;
bool IsPhase6DCampaignMissionControl(DWORD page, WORD valueLink) noexcept;
DWORD SelectCampaignCatalogOwner(
    const std::array<bool, kCampaignCatalogPages.size()>& active) noexcept;

struct CampaignMenuFeature final {
    DWORD surfaceWidth = 0u;
    DWORD surfaceHeight = 0u;
    WORD gfxCollection = 0u;
    WORD containerType = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
    WORD mainTexture = 0u;
    WORD valueLink = 0u;
    WORD buttonPressedTexture = 0u;
    WORD tooltipLink = 0u;
    WORD tooltipLinkExtra = 0u;
    std::uint16_t imageStyle = 0u;
    std::uint16_t effects = 0u;
    std::uint16_t textStyle = 0u;
    WORD showTexture = 0u;
    WORD backTexture = 0u;
    BoundedMenuText text{};
    bool hasText = false;
};

enum class CampaignMenuSnapshotStatus {
    Success,
    Empty,
    Invalid,
};

struct CampaignMenuSnapshot final {
    CampaignMenuSnapshotStatus status = CampaignMenuSnapshotStatus::Empty;
    DWORD page = S4_GUI_UNKNOWN;
    std::array<CampaignMenuFeature, kMaximumCampaignMenuFeatures> features{};
    std::size_t count = 0u;
    std::uint64_t generation = 0u;
};

bool CopyCampaignMenuFeature(LPS4GUIDRAWBLTPARAMS source,
                             CampaignMenuFeature& output) noexcept;
bool EqualCampaignMenuFeature(const CampaignMenuFeature& left,
                              const CampaignMenuFeature& right) noexcept;
bool EqualCampaignMenuSnapshot(const CampaignMenuSnapshot& left,
                               const CampaignMenuSnapshot& right) noexcept;

class CampaignMenuCapture final {
public:
    void SynchronizePage(DWORD page, bool campaignPageActive) noexcept;
    std::optional<CampaignMenuSnapshot> ObserveFrame(
        DWORD page, bool campaignPageActive) noexcept;
    bool ObserveFeature(const CampaignMenuFeature& feature) noexcept;
    void Invalidate() noexcept;
    void Disable() noexcept;
    bool Active() const noexcept { return enabled_ && collecting_; }
    DWORD ActivePage() const noexcept {
        return Active() ? activePage_ : S4_GUI_UNKNOWN;
    }

private:
    void ClearWorking() noexcept;

    std::array<CampaignMenuFeature, kMaximumCampaignMenuFeatures> cached_{};
    std::size_t count_ = 0u;
    std::uint64_t generation_ = 0u;
    DWORD activePage_ = S4_GUI_UNKNOWN;
    bool collecting_ = false;
    bool invalid_ = false;
    bool dirty_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
