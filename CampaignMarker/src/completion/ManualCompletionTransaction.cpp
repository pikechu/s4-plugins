#include "completion/ManualCompletionTransaction.h"

#include <map>
#include <set>

namespace campaign_completion {

ManualCompletionCandidate BuildManualCompletionCandidate(
    const CompletionDatabaseSnapshot& baseline,
    std::uint64_t currentRevision,
    const ManualCompletionRequest& request) noexcept {
    ManualCompletionCandidate result{};
    try {
        if (request.baselineRevision != currentRevision) {
            result.status = ManualCompletionStatus::Conflict;
            return result;
        }
        if (request.entries.size() > kMaximumManualCompletionEntries) {
            return result;
        }

        std::map<std::string, const ManualCompletionEntry*> changes;
        for (const auto& entry : request.entries) {
            if (!entry.editable || entry.stableId.empty() ||
                entry.stableId != entry.record.stableId ||
                !changes.emplace(entry.stableId, &entry).second) {
                return result;
            }
            const auto relative =
                StrictUtf8ToWide(entry.record.relativeId);
            if (!relative.has_value() ||
                CompletionKindFor(*relative) ==
                    CompletionMapKind::Random ||
                entry.record.launchSource == LaunchSource::RandomMap) {
                return result;
            }
        }

        std::set<std::string> existing;
        for (const auto& record : baseline.records) {
            if (!existing.insert(record.stableId).second) return result;
            const auto change = changes.find(record.stableId);
            if (change == changes.end() ||
                change->second->desiredCompleted) {
                result.snapshot.records.push_back(record);
            }
        }
        for (const auto& change : changes) {
            if (existing.find(change.first) != existing.end()) continue;
            if (!change.second->desiredCompleted ||
                change.second->record.recordSource !=
                    kManualCompletionRecordSource) {
                result.snapshot = {};
                return result;
            }
            result.snapshot.records.push_back(change.second->record);
        }

        const auto baselineBytes = EncodeCompletionJson(baseline);
        const auto candidateBytes = EncodeCompletionJson(result.snapshot);
        if (!baselineBytes.has_value() ||
            !candidateBytes.has_value()) {
            result.snapshot = {};
            return result;
        }
        result.status = *baselineBytes == *candidateBytes
                            ? ManualCompletionStatus::Unchanged
                            : ManualCompletionStatus::Changed;
        return result;
    } catch (...) {
        return {};
    }
}

}  // namespace campaign_completion
