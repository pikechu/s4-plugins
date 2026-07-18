#include "manager/CompletionManagerCatalog.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace campaign_completion {
namespace {

CompletionManagerCategory CampaignCategory(
    const CampaignDescriptorRecord& descriptor) noexcept {
    switch (descriptor.page) {
        case S4_SCREEN_ADDON_TROJAN:
            return CompletionManagerCategory::CampaignAddOnTrojan;
        case S4_SCREEN_ADDON_ROMAN:
            return CompletionManagerCategory::CampaignAddOnRoman;
        case S4_SCREEN_ADDON_MAYAN:
            return CompletionManagerCategory::CampaignAddOnMayan;
        case S4_SCREEN_ADDON_VIKING:
            return CompletionManagerCategory::CampaignAddOnViking;
        case S4_SCREEN_ADDON_SETTLE:
            return CompletionManagerCategory::CampaignAddOnSettlement;
        case S4_SCREEN_MISSIONCD:
            return CompletionManagerCategory::CampaignMissionCdBonus;
        case S4_SCREEN_MISSIONCD_ROMAN:
            return CompletionManagerCategory::CampaignMissionCdRoman;
        case S4_SCREEN_MISSIONCD_VIKING:
            return CompletionManagerCategory::CampaignMissionCdViking;
        case S4_SCREEN_MISSIONCD_MAYAN:
            return CompletionManagerCategory::CampaignMissionCdMayan;
        case S4_SCREEN_MISSIONCD_CONFL:
            return CompletionManagerCategory::CampaignMissionCdConflict;
        default:
            break;
    }
    switch (descriptor.group) {
        case CampaignDescriptorGroup::AddOn:
        case CampaignDescriptorGroup::MissionCd:
            return CompletionManagerCategory::HistoricalUnavailable;
        case CampaignDescriptorGroup::NewWorld:
            return CompletionManagerCategory::CampaignNewWorld;
        case CampaignDescriptorGroup::NewWorld2:
            return CompletionManagerCategory::CampaignNewWorld2;
        case CampaignDescriptorGroup::Original:
            return CompletionManagerCategory::CampaignOriginal;
        case CampaignDescriptorGroup::DarkTribe:
            return CompletionManagerCategory::CampaignDarkTribe;
        case CampaignDescriptorGroup::Count:
            return CompletionManagerCategory::HistoricalUnavailable;
    }
    return CompletionManagerCategory::HistoricalUnavailable;
}

LaunchSource SourceFor(CompletionManagerCategory category) noexcept {
    switch (category) {
        case CompletionManagerCategory::FixedMultiplayerList:
            return LaunchSource::SinglePlayerMultiplayerMap;
        case CompletionManagerCategory::FixedCustom:
            return LaunchSource::SinglePlayerCustomMap;
        case CompletionManagerCategory::FixedSinglePlayer:
            return LaunchSource::SinglePlayerMap;
        default:
            return LaunchSource::Unknown;
    }
}

std::wstring CampaignLabel(const CampaignDescriptorRecord& descriptor) {
    std::wstring label = L"Mission ";
    label += std::to_wstring(
        static_cast<unsigned>(descriptor.ordinal) + 1u);
    return label;
}

bool IsFixedCategory(CompletionManagerCategory category) noexcept {
    return category == CompletionManagerCategory::FixedSinglePlayer ||
           category == CompletionManagerCategory::FixedMultiplayerList ||
           category == CompletionManagerCategory::FixedCustom;
}

}  // namespace

CompletionManagerCatalogResult BuildCompletionManagerCatalog(
    const CampaignDescriptorCatalog& descriptors,
    const std::vector<DiscoveredFixedMap>& maps,
    const CompletionDatabaseSnapshot& database) noexcept {
    CompletionManagerCatalogResult result{};
    try {
        std::map<std::string, const CompletionRecord*> completed;
        for (const auto& record : database.records) {
            if (!completed.emplace(record.stableId, &record).second) {
                return result;
            }
        }

        std::map<std::string, std::vector<std::size_t>> identities;
        if (descriptors.executableAdmitted) {
            for (const auto& descriptor : AllCampaignDescriptors()) {
                if (!descriptors.GroupAdmitted(descriptor.group) ||
                    descriptor.relative == nullptr) {
                    continue;
                }
                const auto stable = BuildStableMapId(descriptor.relative);
                if (!stable.has_value()) return result;
                CompletionManagerEntry entry{};
                entry.stableId = *stable;
                entry.relativeId = descriptor.relative;
                entry.displayName = CampaignLabel(descriptor);
                entry.category = CampaignCategory(descriptor);
                entry.kind = CompletionManagerEntryKind::Campaign;
                entry.mapKind = CompletionMapKind::Fixed;
                entry.launchSource = LaunchSource::Campaign;
                entry.checked =
                    completed.find(entry.stableId) != completed.end();
                if (entry.checked) {
                    entry.completedRecord =
                        *completed.find(entry.stableId)->second;
                }
                entry.editable = true;
                entry.available = true;
                identities[entry.stableId].push_back(
                    result.entries.size());
                result.entries.push_back(std::move(entry));
            }
        }

        for (const auto& map : maps) {
            if (!IsFixedCategory(map.category) ||
                map.relativeId.empty()) {
                return result;
            }
            const auto stable = BuildStableMapId(map.relativeId);
            if (!stable.has_value() ||
                CompletionKindFor(map.relativeId) !=
                    CompletionMapKind::Fixed) {
                return result;
            }
            CompletionManagerEntry entry{};
            entry.stableId = *stable;
            entry.relativeId = map.relativeId;
            entry.displayName = map.displayName;
            entry.category = map.category;
            entry.kind = CompletionManagerEntryKind::FixedMap;
            entry.mapKind = CompletionMapKind::Fixed;
            entry.launchSource = SourceFor(map.category);
            entry.checked =
                completed.find(entry.stableId) != completed.end();
            if (entry.checked) {
                entry.completedRecord =
                    *completed.find(entry.stableId)->second;
            }
            entry.editable = true;
            entry.available = true;
            identities[entry.stableId].push_back(result.entries.size());
            result.entries.push_back(std::move(entry));
        }

        for (const auto& record : database.records) {
            if (identities.find(record.stableId) != identities.end()) {
                continue;
            }
            const auto relative = StrictUtf8ToWide(record.relativeId);
            if (!relative.has_value()) return result;
            CompletionManagerEntry entry{};
            entry.stableId = record.stableId;
            entry.relativeId = *relative;
            const auto display = StrictUtf8ToWide(record.displayName);
            if (!display.has_value()) return result;
            entry.displayName = *display;
            entry.mapKind = record.mapKind;
            entry.launchSource = record.launchSource;
            entry.completedRecord = record;
            entry.checked = true;
            if (record.mapKind == CompletionMapKind::Random ||
                record.launchSource == LaunchSource::RandomMap) {
                entry.category =
                    CompletionManagerCategory::RandomMarkerHidden;
                entry.kind = CompletionManagerEntryKind::Random;
                entry.editable = false;
            } else {
                entry.category =
                    CompletionManagerCategory::HistoricalUnavailable;
                entry.kind = CompletionManagerEntryKind::Historical;
                entry.editable = true;
            }
            entry.available = false;
            identities[entry.stableId].push_back(result.entries.size());
            result.entries.push_back(std::move(entry));
        }

        for (const auto& identity : identities) {
            if (identity.second.size() < 2u) continue;
            result.status = CompletionManagerCatalogStatus::Ambiguous;
            result.ambiguous += identity.second.size();
            for (const auto index : identity.second) {
                result.entries[index].ambiguous = true;
                result.entries[index].editable = false;
            }
        }
        if (result.status != CompletionManagerCatalogStatus::Ambiguous) {
            result.status = CompletionManagerCatalogStatus::Success;
        }
        std::sort(
            result.entries.begin(), result.entries.end(),
            [](const CompletionManagerEntry& left,
               const CompletionManagerEntry& right) {
                if (left.category != right.category) {
                    return left.category < right.category;
                }
                return left.stableId < right.stableId;
            });
        return result;
    } catch (...) {
        return {};
    }
}

const wchar_t* CompletionManagerCategoryLabel(
    CompletionManagerCategory category) noexcept {
    switch (category) {
        case CompletionManagerCategory::CampaignAddOnTrojan:
            return L"Campaigns / Add-on / Trojan";
        case CompletionManagerCategory::CampaignAddOnRoman:
            return L"Campaigns / Add-on / Roman";
        case CompletionManagerCategory::CampaignAddOnMayan:
            return L"Campaigns / Add-on / Mayan";
        case CompletionManagerCategory::CampaignAddOnViking:
            return L"Campaigns / Add-on / Viking";
        case CompletionManagerCategory::CampaignAddOnSettlement:
            return L"Campaigns / Add-on / Settlement";
        case CompletionManagerCategory::CampaignMissionCdBonus:
            return L"Campaigns / Mission CD / Bonus";
        case CompletionManagerCategory::CampaignMissionCdRoman:
            return L"Campaigns / Mission CD / Roman";
        case CompletionManagerCategory::CampaignMissionCdViking:
            return L"Campaigns / Mission CD / Viking";
        case CompletionManagerCategory::CampaignMissionCdMayan:
            return L"Campaigns / Mission CD / Mayan";
        case CompletionManagerCategory::CampaignMissionCdConflict:
            return L"Campaigns / Mission CD / Conflict";
        case CompletionManagerCategory::CampaignNewWorld:
            return L"Campaigns / New World";
        case CompletionManagerCategory::CampaignNewWorld2:
            return L"Campaigns / New World 2";
        case CompletionManagerCategory::CampaignOriginal:
            return L"Campaigns / Original";
        case CompletionManagerCategory::CampaignDarkTribe:
            return L"Campaigns / Dark Tribe";
        case CompletionManagerCategory::FixedSinglePlayer:
            return L"Maps / Single Player";
        case CompletionManagerCategory::FixedMultiplayerList:
            return L"Maps / Multiplayer List";
        case CompletionManagerCategory::FixedCustom:
            return L"Maps / Custom";
        case CompletionManagerCategory::HistoricalUnavailable:
            return L"Historical / Unavailable";
        case CompletionManagerCategory::RandomMarkerHidden:
            return L"Random Maps / Marker Hidden";
    }
    return L"Unknown";
}

}  // namespace campaign_completion
