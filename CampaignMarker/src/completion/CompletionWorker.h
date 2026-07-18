#pragma once

#include "completion/CompletionCandidateCoordinator.h"
#include "completion/CompletionStore.h"
#include "diagnostics/Logger.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace campaign_completion {

class CompletionWorker final {
public:
    using LogSink = std::function<void(LogLevel, std::string)>;
    using SnapshotSink =
        std::function<void(CompletionDatabaseSnapshot)>;

    CompletionWorker(ICompletionStore& store,
                     LogSink log,
                     SnapshotSink publish);
    CompletionWorker(const CompletionWorker&) = delete;
    CompletionWorker& operator=(const CompletionWorker&) = delete;
    ~CompletionWorker();

    bool Start() noexcept;
    bool TryEnqueue(CompletionCandidate candidate) noexcept;
    bool StopAndDrain(std::chrono::milliseconds timeout) noexcept;
    bool Running() const noexcept;

private:
    static constexpr std::size_t kCapacity = 32u;

    void Run() noexcept;
    void SafeLog(LogLevel level, std::string message) noexcept;

    ICompletionStore& store_;
    LogSink log_;
    SnapshotSink publish_;
    mutable std::mutex mutex_;
    std::condition_variable workCondition_;
    std::condition_variable finishedCondition_;
    std::deque<CompletionCandidate> queue_;
    std::thread thread_;
    std::size_t outstanding_ = 0u;
    bool started_ = false;
    bool accepting_ = false;
    bool stopRequested_ = false;
    bool finished_ = false;
};

}  // namespace campaign_completion
