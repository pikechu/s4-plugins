#include "completion/CompletionWorker.h"
#include "campaign/CampaignCompletionMarkerIndex.h"
#include "campaign/CampaignDescriptorCatalog.h"
#include "marker/CompletionMarkerIndex.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::CompletionCandidate Candidate(std::uint64_t id) {
    campaign_completion::CompletionCandidate candidate{};
    candidate.sessionId = id;
    candidate.record.stableId = "map:test-" + std::to_string(id);
    candidate.record.relativeId = "test-" + std::to_string(id);
    candidate.record.displayName = "test";
    candidate.record.completedAtUtc = "2026-07-14T01:57:38Z";
    return candidate;
}

class ControllableStore final : public campaign_completion::ICompletionStore {
public:
    campaign_completion::CompletionAddResult AddIfAbsent(
        const campaign_completion::CompletionRecord& record) noexcept override {
        try {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            enteredCondition.notify_all();
            while (blocked) {
                releaseCondition.wait(lock);
            }
            records.push_back(record.stableId);
            if (record.stableId.find("fail") != std::string::npos) {
                return {campaign_completion::CompletionAddStatus::Failed,
                        campaign_completion::CompletionTransactionStage::ReplaceMain,
                        ERROR_WRITE_FAULT};
            }
            if (record.stableId.find("readonly") != std::string::npos) {
                return {campaign_completion::CompletionAddStatus::ReadOnly,
                        campaign_completion::CompletionTransactionStage::None,
                        ERROR_ACCESS_DENIED};
            }
            if (record.stableId.find("duplicate") != std::string::npos) {
                return {campaign_completion::CompletionAddStatus::Duplicate,
                        campaign_completion::CompletionTransactionStage::None,
                        ERROR_SUCCESS};
            }
            snapshot.records.push_back(record);
            return {campaign_completion::CompletionAddStatus::Committed,
                    campaign_completion::CompletionTransactionStage::None,
                    ERROR_SUCCESS};
        } catch (...) {
            return {campaign_completion::CompletionAddStatus::Failed,
                    campaign_completion::CompletionTransactionStage::Encode,
                    ERROR_OUTOFMEMORY};
        }
    }

    campaign_completion::CompletionDatabaseSnapshot Snapshot()
        const override {
        std::lock_guard<std::mutex> lock(mutex);
        ++snapshotCalls;
        return snapshot;
    }

    bool WaitUntilEntered(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        return enteredCondition.wait_for(lock, timeout,
                                         [this] { return entered; });
    }

    void Release() {
        std::lock_guard<std::mutex> lock(mutex);
        blocked = false;
        releaseCondition.notify_all();
    }

    mutable std::mutex mutex;
    std::condition_variable enteredCondition;
    std::condition_variable releaseCondition;
    std::vector<std::string> records;
    campaign_completion::CompletionDatabaseSnapshot snapshot;
    mutable std::size_t snapshotCalls = 0u;
    bool blocked = false;
    bool entered = false;
};

}  // namespace

int RunCompletionWorkerTests() {
    using namespace campaign_completion;
    using namespace std::chrono_literals;

    {
        ControllableStore store;
        store.blocked = true;
        std::vector<std::string> logs;
        CompletionWorker worker(
            store, [&logs](LogLevel, std::string line) {
                logs.push_back(std::move(line));
            }, {});
        Require(worker.Start() && worker.Running(),
                "worker must start exactly one consumer");
        Require(!worker.Start(), "worker must reject a second start");
        Require(worker.TryEnqueue(Candidate(1u)),
                "first candidate must enter the worker");
        Require(store.WaitUntilEntered(2s),
                "consumer must begin the first store operation");
        for (std::uint64_t id = 2u; id <= 32u; ++id) {
            Require(worker.TryEnqueue(Candidate(id)),
                    "first 32 outstanding candidates must fit");
        }
        const auto fullStart = std::chrono::steady_clock::now();
        Require(!worker.TryEnqueue(Candidate(33u)),
                "33rd outstanding candidate must be rejected");
        Require(std::chrono::steady_clock::now() - fullStart < 100ms,
                "full queue rejection must remain nonblocking");
        store.Release();
        Require(worker.StopAndDrain(5s),
                "released queue must drain and join");
        Require(!worker.Running(), "drained worker must stop running");
        Require(store.records.size() == 32u,
                "all accepted candidates must reach the store");
        for (std::size_t index = 0u; index < store.records.size(); ++index) {
            Require(store.records[index] ==
                        "map:test-" + std::to_string(index + 1u),
                    "worker must preserve FIFO order");
        }
        Require(!worker.TryEnqueue(Candidate(34u)),
                "stopped worker must reject new admission");
    }

    {
        ControllableStore store;
        std::vector<std::string> logs;
        CompletionWorker worker(
            store, [&logs](LogLevel, std::string line) {
                logs.push_back(std::move(line));
            }, {});
        Require(worker.Start(), "result logging worker must start");
        auto duplicate = Candidate(40u);
        duplicate.record.stableId = "map:duplicate";
        auto failure = Candidate(41u);
        failure.record.stableId = "map:fail";
        Require(worker.TryEnqueue(std::move(duplicate)) &&
                    worker.TryEnqueue(std::move(failure)),
                "result logging candidates must enqueue");
        Require(worker.StopAndDrain(5s),
                "result logging worker must drain");
        bool sawFailure = false;
        bool duplicateWasError = false;
        for (const auto& line : logs) {
            sawFailure = sawFailure ||
                (line.find("map:fail") != std::string::npos &&
                 line.find("stage=replace-main") != std::string::npos &&
                 line.find("error=29") != std::string::npos);
            duplicateWasError = duplicateWasError ||
                (line.find("map:duplicate") != std::string::npos &&
                 line.find("failed") != std::string::npos);
        }
        Require(sawFailure,
                "store failure log must contain stable ID, stage, and error");
        Require(!duplicateWasError,
                "duplicate store result must not be logged as an error");
    }

    {
        ControllableStore store;
        CompletionWorker worker(
            store, [](LogLevel, std::string) {
                throw std::runtime_error("log sink failure");
            }, {});
        Require(worker.Start() && worker.TryEnqueue(Candidate(50u)) &&
                    worker.StopAndDrain(5s),
                "log sink exception must not escape or stop draining");
    }

    {
        ControllableStore store;
        store.blocked = true;
        CompletionWorker worker(store, [](LogLevel, std::string) {}, {});
        Require(worker.Start() && worker.TryEnqueue(Candidate(60u)) &&
                    store.WaitUntilEntered(2s),
                "timeout fixture must block inside store");
        const auto before = std::chrono::steady_clock::now();
        Require(!worker.StopAndDrain(50ms),
                "blocked store must report bounded drain timeout");
        const auto elapsed = std::chrono::steady_clock::now() - before;
        Require(elapsed >= 40ms && elapsed < 1s,
                "drain timeout must be bounded near the requested interval");
        Require(!worker.TryEnqueue(Candidate(61u)),
                "timed-out shutdown must keep admission closed");
        store.Release();
        Require(worker.StopAndDrain(5s),
                "retained worker must support a later successful drain");
    }

    {
        ControllableStore store;
        std::vector<CompletionDatabaseSnapshot> publications;
        CompletionWorker worker(
            store, [](LogLevel, std::string) {},
            [&publications](CompletionDatabaseSnapshot snapshot) {
                publications.push_back(std::move(snapshot));
            });
        Require(worker.Start() && worker.TryEnqueue(Candidate(70u)) &&
                    worker.StopAndDrain(5s),
                "committed publication worker must drain");
        Require(store.snapshotCalls == 1u && publications.size() == 1u &&
                    publications.front().records.size() == 1u &&
                    publications.front().records.front().stableId ==
                        "map:test-70",
                "committed transaction publishes the replacement snapshot once");
    }

    {
        ControllableStore store;
        CompletionMarkerIndex fixedIndex;
        CampaignCompletionMarkerIndex campaignIndex;
        CampaignDescriptorCatalog catalog{};
        catalog.executableAdmitted = true;
        catalog.groups.fill(true);
        const auto& descriptor = AllCampaignDescriptors()[16u];
        CompletionWorker worker(
            store, [](LogLevel, std::string) {},
            [&fixedIndex, &campaignIndex, &catalog](
                CompletionDatabaseSnapshot snapshot) {
                fixedIndex.Publish(snapshot);
                campaignIndex.Publish(snapshot, catalog);
            });
        auto candidate = Candidate(73u);
        candidate.record.stableId =
            BuildStableMapId(descriptor.relative).value();
        candidate.record.relativeId =
            WideToStrictUtf8(descriptor.relative).value();
        candidate.record.launchSource = LaunchSource::Campaign;
        Require(worker.Start() &&
                    worker.TryEnqueue(std::move(candidate)) &&
                    worker.StopAndDrain(5s),
                "campaign publication worker must drain");
        Require(campaignIndex.Match(descriptor) ==
                    CampaignMarkerMatchStatus::Unique,
                "committed campaign victory refreshes its marker index in-process");
    }

    for (const auto& stableId : {
             std::string("map:duplicate"),
             std::string("map:readonly"),
             std::string("map:fail")}) {
        ControllableStore store;
        std::vector<CompletionDatabaseSnapshot> publications;
        CompletionWorker worker(
            store, [](LogLevel, std::string) {},
            [&publications](CompletionDatabaseSnapshot snapshot) {
                publications.push_back(std::move(snapshot));
            });
        auto candidate = Candidate(71u);
        candidate.record.stableId = stableId;
        Require(worker.Start() && worker.TryEnqueue(std::move(candidate)) &&
                    worker.StopAndDrain(5s),
                "non-committed publication worker must drain");
        Require(store.snapshotCalls == 0u && publications.empty(),
                "duplicate, read-only, and failed results never publish");
    }

    {
        ControllableStore store;
        std::vector<std::string> logs;
        CompletionWorker worker(
            store,
            [&logs](LogLevel, std::string line) {
                logs.push_back(std::move(line));
            },
            [](CompletionDatabaseSnapshot) {
                throw std::runtime_error("snapshot sink failure");
            });
        Require(worker.Start() && worker.TryEnqueue(Candidate(72u)) &&
                    worker.StopAndDrain(5s),
                "snapshot sink failure must not stop worker drain");
        bool sawCommitted = false;
        bool sawPublishFailure = false;
        for (const auto& line : logs) {
            sawCommitted = sawCommitted ||
                line.find("completion-store committed") != std::string::npos;
            sawPublishFailure = sawPublishFailure ||
                line == "completion-marker-index publish-failed";
        }
        Require(store.snapshotCalls == 1u &&
                    store.snapshot.records.size() == 1u && sawCommitted &&
                    sawPublishFailure,
                "publication failure preserves the committed result and logs once");
    }
    return 0;
}
