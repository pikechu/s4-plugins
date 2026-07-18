#include "campaign/CampaignMarkerObserver.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int RunCampaignMarkerObserverTests() {
    using namespace campaign_completion;

    Require(kMaximumMarkerCommands >= 36u,
            "the shared marker frame cannot truncate the composite page");

    CampaignDescriptorCatalog catalog{};
    catalog.executableAdmitted = true;
    catalog.groups.fill(true);
    CompletionDatabaseSnapshot allCompleted{};
    for (const auto& descriptor : AllCampaignDescriptors()) {
        CompletionRecord record{};
        const auto relative = WideToStrictUtf8(descriptor.relative);
        const auto stable = BuildStableMapId(descriptor.relative);
        Require(relative.has_value() && stable.has_value(),
                "descriptor fixture encodes");
        record.stableId = *stable;
        record.relativeId = *relative;
        record.displayName = "RD_PlayerSave";
        record.mapKind = CompletionMapKind::Fixed;
        record.launchSource = LaunchSource::Campaign;
        record.completedAtUtc = "2026-07-18T00:00:00Z";
        allCompleted.records.push_back(std::move(record));
    }
    CampaignCompletionMarkerIndex index;
    index.Publish(allCompleted, catalog);
    CampaignMarkerObserver observer(catalog, index);

    CampaignMenuSnapshot composite{};
    composite.status = CampaignMenuSnapshotStatus::Success;
    composite.page = S4_SCREEN_NEWWORLD2;
    composite.generation = 1u;
    for (const auto& descriptor : AllCampaignDescriptors()) {
        if (descriptor.page != composite.page) continue;
        auto& feature = composite.features[composite.count++];
        feature.surfaceWidth = 800u;
        feature.surfaceHeight = 600u;
        feature.valueLink = descriptor.control.controlId;
        feature.x = descriptor.control.x;
        feature.y = descriptor.control.y;
        feature.width = descriptor.control.width;
        feature.height = descriptor.control.height;
    }
    Require(composite.count == 36u,
            "the composite fixture contains all simultaneous controls");
    observer.ObserveSnapshot(composite);
    const auto frame =
        observer.TakeFrame(S4_SCREEN_NEWWORLD2, S4_SCREEN_NEWWORLD2);
    Require(frame.count == 36u,
            "all-completed composite page draws without truncation or hover");
    Require(observer.TakeFrame(S4_SCREEN_NEWWORLD, S4_SCREEN_NEWWORLD2).count ==
                0u,
            "a non-owner callback cannot draw the retained campaign frame");

    observer.ObservePage(S4_GUI_UNKNOWN);
    Require(observer.TakeFrame(S4_SCREEN_NEWWORLD2,
                               S4_GUI_UNKNOWN).count == 0u,
            "leaving campaign pages clears retained commands");

    auto invalid = composite;
    invalid.status = CampaignMenuSnapshotStatus::Invalid;
    observer.ObserveSnapshot(invalid);
    Require(observer.TakeFrame(S4_SCREEN_NEWWORLD2,
                               S4_SCREEN_NEWWORLD2).count == 0u,
            "invalid snapshots fail closed");

    return 0;
}
