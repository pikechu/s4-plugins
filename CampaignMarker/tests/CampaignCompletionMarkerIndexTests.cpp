#include "campaign/CampaignCompletionMarkerIndex.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::CampaignDescriptorCatalog FullCatalog() {
    using namespace campaign_completion;
    ModuleInfo module{};
    module.version = "2.50.1516.0";
    module.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    CampaignDescriptorEvidence evidence{};
    evidence.addOn = evidence.missionCd = evidence.newWorld =
        evidence.newWorld2 = evidence.original = evidence.darkTribe = true;
    return AdmitCampaignDescriptorCatalog(module, evidence);
}

campaign_completion::CompletionRecord Completed(
    const campaign_completion::CampaignDescriptorRecord& descriptor) {
    using namespace campaign_completion;
    CompletionRecord record{};
    const auto relative = WideToStrictUtf8(descriptor.relative);
    const auto stable = BuildStableMapId(descriptor.relative);
    if (!relative.has_value() || !stable.has_value()) {
        throw std::runtime_error("descriptor fixture failed to encode");
    }
    record.stableId = *stable;
    record.relativeId = *relative;
    record.displayName = "RD_PlayerSave";
    record.mapKind = CompletionMapKind::Fixed;
    record.launchSource = LaunchSource::Campaign;
    record.completedAtUtc = "2026-07-18T00:00:00Z";
    return record;
}

}  // namespace

int RunCampaignCompletionMarkerIndexTests() {
    using namespace campaign_completion;

    const auto catalog = FullCatalog();
    CompletionDatabaseSnapshot allCompleted{};
    for (const auto& descriptor : AllCampaignDescriptors()) {
        allCompleted.records.push_back(Completed(descriptor));
    }

    CampaignCompletionMarkerIndex index;
    const auto summary = index.Publish(allCompleted, catalog);
    Require(summary.admitted == 107u && summary.rejected == 0u &&
                summary.ambiguous == 0u,
            "all 107 exact campaign records enter the separate index");
    for (const auto& descriptor : AllCampaignDescriptors()) {
        Require(index.Match(descriptor) == CampaignMarkerMatchStatus::Unique,
                "every closed descriptor matches allocation-free");
    }

    auto wrongSource = Completed(AllCampaignDescriptors().front());
    wrongSource.launchSource = LaunchSource::SinglePlayerMap;
    auto wrongKind = Completed(AllCampaignDescriptors()[1u]);
    wrongKind.mapKind = CompletionMapKind::Random;
    auto wrongStable = Completed(AllCampaignDescriptors()[2u]);
    wrongStable.stableId = "map:wrong";
    auto wrongCase = Completed(AllCampaignDescriptors()[3u]);
    wrongCase.relativeId[0] = 'm';
    CompletionDatabaseSnapshot rejected{};
    rejected.records = {wrongSource, wrongKind, wrongStable, wrongCase};
    const auto rejectedSummary = index.Publish(rejected, catalog);
    Require(rejectedSummary.admitted == 0u &&
                rejectedSummary.rejected == 4u,
            "wrong source, kind, stable ID, and exact-case relative fail closed");

    auto duplicate = Completed(AllCampaignDescriptors().front());
    CompletionDatabaseSnapshot ambiguous{};
    ambiguous.records = {duplicate, duplicate};
    const auto ambiguousSummary = index.Publish(ambiguous, catalog);
    Require(ambiguousSummary.ambiguous == 1u &&
                index.Match(AllCampaignDescriptors().front()) ==
                    CampaignMarkerMatchStatus::Ambiguous,
            "duplicate valid records suppress a campaign marker");

    return 0;
}
