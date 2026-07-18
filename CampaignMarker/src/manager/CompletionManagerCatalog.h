#pragma once

#include "campaign/CampaignDescriptorCatalog.h"
#include "completion/CompletionJson.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace campaign_completion {

enum class CompletionManagerCategory : std::uint8_t {
    CampaignAddOnTrojan,
    CampaignAddOnRoman,
    CampaignAddOnMayan,
    CampaignAddOnViking,
    CampaignAddOnSettlement,
    CampaignMissionCdBonus,
    CampaignMissionCdRoman,
    CampaignMissionCdViking,
    CampaignMissionCdMayan,
    CampaignMissionCdConflict,
    CampaignNewWorld,
    CampaignNewWorld2,
    CampaignOriginal,
    CampaignDarkTribe,
    FixedSinglePlayer,
    FixedMultiplayerList,
    FixedCustom,
    HistoricalUnavailable,
    RandomMarkerHidden,
};

enum class CompletionManagerEntryKind : std::uint8_t {
    Campaign,
    FixedMap,
    Historical,
    Random,
};

struct DiscoveredFixedMap final {
    std::wstring relativeId;
    std::wstring displayName;
    CompletionManagerCategory category =
        CompletionManagerCategory::FixedSinglePlayer;
};

struct CompletionManagerEntry final {
    std::string stableId;
    std::wstring relativeId;
    std::wstring displayName;
    CompletionManagerCategory category =
        CompletionManagerCategory::HistoricalUnavailable;
    CompletionManagerEntryKind kind =
        CompletionManagerEntryKind::Historical;
    CompletionMapKind mapKind = CompletionMapKind::Fixed;
    LaunchSource launchSource = LaunchSource::Unknown;
    CompletionRecord completedRecord;
    bool checked = false;
    bool editable = false;
    bool available = false;
    bool ambiguous = false;
};

enum class CompletionManagerCatalogStatus : std::uint8_t {
    Success,
    Ambiguous,
    Invalid,
};

struct CompletionManagerCatalogResult final {
    std::vector<CompletionManagerEntry> entries;
    CompletionManagerCatalogStatus status =
        CompletionManagerCatalogStatus::Invalid;
    std::size_t ambiguous = 0u;
};

CompletionManagerCatalogResult BuildCompletionManagerCatalog(
    const CampaignDescriptorCatalog& descriptors,
    const std::vector<DiscoveredFixedMap>& maps,
    const CompletionDatabaseSnapshot& database) noexcept;
const wchar_t* CompletionManagerCategoryLabel(
    CompletionManagerCategory category) noexcept;

}  // namespace campaign_completion
