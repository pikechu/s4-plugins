#include "completion/ManualCompletionTransaction.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::CompletionRecord Record(
    std::string relative, std::string source = "manual-manager") {
    using namespace campaign_completion;
    CompletionRecord record{};
    record.relativeId = std::move(relative);
    const auto wide = StrictUtf8ToWide(record.relativeId);
    record.stableId = BuildStableMapId(*wide).value();
    record.displayName = record.relativeId;
    record.mapKind = CompletionKindFor(*wide);
    record.launchSource =
        record.mapKind == CompletionMapKind::Random
            ? LaunchSource::RandomMap
            : LaunchSource::Campaign;
    record.completedAtUtc = "2026-07-18T10:00:00Z";
    record.recordSource = std::move(source);
    return record;
}

}  // namespace

int RunManualCompletionTransactionTests() {
    using namespace campaign_completion;

    CompletionDatabaseSnapshot baseline{};
    auto automatic = Record("Map\\Campaign\\md_roman1.map",
                            "native-event-609");
    baseline.records.push_back(automatic);

    ManualCompletionRequest request{};
    request.baselineRevision = 7u;
    request.entries.push_back(
        {automatic.stableId, automatic, false, true});
    auto trojan = Record("Map\\Campaign\\ao_trojan01.map");
    request.entries.push_back({trojan.stableId, trojan, true, true});

    const auto applied =
        BuildManualCompletionCandidate(baseline, 7u, request);
    Require(applied.status == ManualCompletionStatus::Changed &&
                applied.snapshot.records.size() == 1u &&
                applied.snapshot.records.front().stableId ==
                    trojan.stableId &&
                applied.snapshot.records.front().recordSource ==
                    "manual-manager",
            "one batch can remove and add exact editable IDs");

    const auto stale =
        BuildManualCompletionCandidate(baseline, 8u, request);
    Require(stale.status == ManualCompletionStatus::Conflict &&
                stale.snapshot.records.empty(),
            "stale manager revision fails before producing a candidate");

    ManualCompletionRequest noChange{};
    noChange.baselineRevision = 7u;
    noChange.entries.push_back(
        {automatic.stableId, automatic, true, true});
    const auto unchanged =
        BuildManualCompletionCandidate(baseline, 7u, noChange);
    Require(unchanged.status == ManualCompletionStatus::Unchanged,
            "unchanged checkbox state produces no transaction");

    auto random = Record("RD_Test");
    ManualCompletionRequest forbidden{};
    forbidden.baselineRevision = 7u;
    forbidden.entries.push_back(
        {random.stableId, random, false, false});
    const auto rejected =
        BuildManualCompletionCandidate(baseline, 7u, forbidden);
    Require(rejected.status == ManualCompletionStatus::Invalid,
            "read-only random rows cannot enter a manual command");

    auto forgedAutomatic = trojan;
    forgedAutomatic.recordSource = std::string(kCompletionRecordSource);
    ManualCompletionRequest forged{};
    forged.baselineRevision = 7u;
    forged.entries.push_back(
        {forgedAutomatic.stableId, forgedAutomatic, true, true});
    const auto forgedRejected =
        BuildManualCompletionCandidate(baseline, 7u, forged);
    Require(forgedRejected.status == ManualCompletionStatus::Invalid,
            "new manual checks cannot forge native-event records");

    return 0;
}
