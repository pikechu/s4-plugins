#pragma once

#include "campaign/CampaignLaunchAssociation.h"
#include "diagnostics/ModuleInventory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace campaign_completion {

enum class CampaignDescriptorGroup : std::uint8_t {
    AddOn,
    MissionCd,
    NewWorld,
    NewWorld2,
    Original,
    DarkTribe,
    Count,
};

inline constexpr std::size_t kPhase6DCampaignDescriptorCount = 107u;

struct CampaignDescriptorRecord final {
    const char* key = nullptr;
    CampaignDescriptorGroup group = CampaignDescriptorGroup::AddOn;
    DWORD page = S4_GUI_UNKNOWN;
    CampaignControlIdentity control{};
    std::uint8_t ordinal = 0u;
    std::uint8_t transitionKind = 0u;
    const wchar_t* relative = nullptr;
};

struct CampaignDescriptorEvidence final {
    bool addOn = false;
    bool missionCd = false;
    bool newWorld = false;
    bool newWorld2 = false;
    bool original = false;
    bool darkTribe = false;
};

struct CampaignDescriptorCatalog final {
    bool executableAdmitted = false;
    std::array<bool, static_cast<std::size_t>(CampaignDescriptorGroup::Count)>
        groups{};

    bool GroupAdmitted(CampaignDescriptorGroup group) const noexcept;
};

enum class CampaignDescriptorValidationStatus : std::uint8_t {
    Matched,
    CatalogNotAdmitted,
    GroupNotAdmitted,
    ControlUnknown,
    GeometryMismatch,
    RelativeMismatch,
};

struct CampaignDescriptorValidation final {
    CampaignDescriptorValidationStatus status =
        CampaignDescriptorValidationStatus::CatalogNotAdmitted;
    const CampaignDescriptorRecord* record = nullptr;
};

CampaignDescriptorEvidence InspectCampaignDescriptorEvidence(
    const ModuleInfo& executable) noexcept;
CampaignDescriptorCatalog AdmitCampaignDescriptorCatalog(
    const ModuleInfo& executable) noexcept;
CampaignDescriptorCatalog AdmitCampaignDescriptorCatalog(
    const ModuleInfo& executable,
    const CampaignDescriptorEvidence& evidence) noexcept;
CampaignDescriptorValidation ValidateCampaignDescriptor(
    const CampaignDescriptorCatalog& catalog,
    const CampaignMenuAssociation& association) noexcept;
const CampaignDescriptorRecord* FindAdmittedCampaignDescriptor(
    const CampaignDescriptorCatalog& catalog, DWORD page,
    const CampaignControlIdentity& control) noexcept;
const CampaignDescriptorRecord* FindAdmittedCampaignDescriptorRelative(
    const CampaignDescriptorCatalog& catalog,
    std::wstring_view relative) noexcept;
const std::array<CampaignDescriptorRecord,
                 kPhase6DCampaignDescriptorCount>&
AllCampaignDescriptors() noexcept;

}  // namespace campaign_completion
