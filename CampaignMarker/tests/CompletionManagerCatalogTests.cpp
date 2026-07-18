#include "manager/CompletionManagerCatalog.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int RunCompletionManagerCatalogTests() {
    using namespace campaign_completion;

    CampaignDescriptorCatalog descriptors{};
    descriptors.executableAdmitted = true;
    descriptors.groups.fill(true);

    std::vector<DiscoveredFixedMap> maps{
        {L"Map\\Singleplayer\\Aeneas.map", L"Aeneas",
         CompletionManagerCategory::FixedSinglePlayer},
        {L"Map\\User\\Antares.map", L"RD_PlayerSave",
         CompletionManagerCategory::FixedCustom},
    };
    CompletionDatabaseSnapshot database{};
    CompletionRecord completed{};
    completed.stableId = "map:map\\campaign\\md_roman1.map";
    completed.relativeId = "Map\\Campaign\\md_roman1.map";
    completed.displayName = "md_roman1";
    completed.mapKind = CompletionMapKind::Fixed;
    completed.launchSource = LaunchSource::Campaign;
    completed.completedAtUtc = "2026-07-18T09:10:32Z";
    database.records.push_back(completed);

    CompletionRecord random = completed;
    random.stableId = "map:rd_test";
    random.relativeId = "RD_Test";
    random.displayName = "random";
    random.mapKind = CompletionMapKind::Random;
    random.launchSource = LaunchSource::RandomMap;
    database.records.push_back(random);

    const auto result =
        BuildCompletionManagerCatalog(descriptors, maps, database);
    Require(result.status == CompletionManagerCatalogStatus::Success,
            "valid classified catalog must build");
    Require(result.entries.size() == 110u,
            "107 campaigns, two fixed maps, and one random record appear once");

    std::size_t campaigns = 0u;
    std::size_t checked = 0u;
    bool antaresFixed = false;
    bool randomReadOnly = false;
    for (const auto& entry : result.entries) {
        campaigns += entry.kind == CompletionManagerEntryKind::Campaign;
        checked += entry.checked;
        if (entry.relativeId == L"Map\\User\\Antares.map") {
            antaresFixed =
                entry.mapKind == CompletionMapKind::Fixed &&
                entry.editable;
        }
        if (entry.relativeId == L"RD_Test") {
            randomReadOnly =
                entry.mapKind == CompletionMapKind::Random &&
                !entry.editable &&
                entry.category ==
                    CompletionManagerCategory::RandomMarkerHidden;
        }
    }
    Require(campaigns == 107u && checked == 2u,
            "catalog joins completion only by exact stable ID");
    Require(antaresFixed,
            "RD-prefixed display text never changes fixed relative identity");
    Require(randomReadOnly,
            "random records are visible but marker-hidden and read-only");

    maps.push_back(
        {L"map\\user\\ANTARES.map", L"collision",
         CompletionManagerCategory::FixedCustom});
    const auto collision =
        BuildCompletionManagerCatalog(descriptors, maps, database);
    Require(collision.status ==
                CompletionManagerCatalogStatus::Ambiguous &&
                collision.ambiguous == 2u,
            "normalized relative collisions fail closed");

    return 0;
}
