#include "completion/CompletionWorker.h"

#include <sstream>
#include <utility>

namespace campaign_completion {
namespace {

const char* StageName(CompletionTransactionStage stage) noexcept {
    switch (stage) {
        case CompletionTransactionStage::None:
            return "none";
        case CompletionTransactionStage::ReadMain:
            return "read-main";
        case CompletionTransactionStage::ReadBackup:
            return "read-backup";
        case CompletionTransactionStage::Encode:
            return "encode";
        case CompletionTransactionStage::WriteTemporary:
            return "write-temporary";
        case CompletionTransactionStage::FlushTemporary:
            return "flush-temporary";
        case CompletionTransactionStage::ReadTemporary:
            return "read-temporary";
        case CompletionTransactionStage::ValidateTemporary:
            return "validate-temporary";
        case CompletionTransactionStage::BackupMain:
            return "backup-main";
        case CompletionTransactionStage::ReplaceMain:
            return "replace-main";
        case CompletionTransactionStage::MoveFirst:
            return "move-first";
    }
    return "unknown";
}

}  // namespace

CompletionWorker::CompletionWorker(ICompletionStore& store,
                                   LogSink log,
                                   SnapshotSink publish)
    : store_(store),
      log_(std::move(log)),
      publish_(std::move(publish)) {}

CompletionWorker::~CompletionWorker() {
    try {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            accepting_ = false;
            stopRequested_ = true;
        }
        workCondition_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    } catch (...) {
        // Runtime integration gives the worker process lifetime. Unit owners must
        // release a blocked store before destruction.
    }
}

bool CompletionWorker::Start() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_) {
            return false;
        }
        started_ = true;
        accepting_ = true;
        stopRequested_ = false;
        finished_ = false;
        outstanding_ = 0u;
        try {
            thread_ = std::thread([this] { Run(); });
        } catch (...) {
            started_ = false;
            accepting_ = false;
            finished_ = true;
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CompletionWorker::TryEnqueue(CompletionCandidate candidate) noexcept {
    try {
        bool full = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!accepting_) {
                return false;
            }
            if (outstanding_ >= kCapacity) {
                full = true;
            } else {
                queue_.push_back(std::move(candidate));
                ++outstanding_;
            }
        }
        if (full) {
            SafeLog(LogLevel::Warning,
                    "completion-worker queue-full capacity=32");
            return false;
        }
        workCondition_.notify_one();
        return true;
    } catch (...) {
        SafeLog(LogLevel::Error, "completion-worker enqueue-failed");
        return false;
    }
}

bool CompletionWorker::StopAndDrain(
    std::chrono::milliseconds timeout) noexcept {
    try {
        std::thread joined;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!started_) {
                return true;
            }
            accepting_ = false;
            stopRequested_ = true;
            workCondition_.notify_all();
            if (!finishedCondition_.wait_for(
                    lock, timeout, [this] { return finished_; })) {
                return false;
            }
            if (thread_.joinable()) {
                joined = std::move(thread_);
            }
        }
        if (joined.joinable()) {
            joined.join();
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CompletionWorker::Running() const noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        return started_ && !finished_;
    } catch (...) {
        return false;
    }
}

void CompletionWorker::Run() noexcept {
    for (;;) {
        CompletionCandidate candidate;
        try {
            std::unique_lock<std::mutex> lock(mutex_);
            workCondition_.wait(lock, [this] {
                return stopRequested_ || !queue_.empty();
            });
            if (queue_.empty()) {
                finished_ = true;
                accepting_ = false;
                lock.unlock();
                finishedCondition_.notify_all();
                return;
            }
            candidate = std::move(queue_.front());
            queue_.pop_front();
        } catch (...) {
            SafeLog(LogLevel::Error, "completion-worker consumer-failed");
            try {
                std::lock_guard<std::mutex> lock(mutex_);
                accepting_ = false;
                finished_ = true;
            } catch (...) {
            }
            finishedCondition_.notify_all();
            return;
        }

        CompletionAddResult result{};
        try {
            result = store_.AddIfAbsent(candidate.record);
        } catch (...) {
            result = {CompletionAddStatus::Failed,
                      CompletionTransactionStage::Encode,
                      ERROR_UNHANDLED_EXCEPTION};
        }

        if (result.status == CompletionAddStatus::Committed && publish_) {
            try {
                auto snapshot = store_.Snapshot();
                publish_(std::move(snapshot));
            } catch (...) {
                SafeLog(LogLevel::Error,
                        "completion-marker-index publish-failed");
            }
        }

        try {
            std::ostringstream line;
            line << "completion-store ";
            LogLevel level = LogLevel::Info;
            switch (result.status) {
                case CompletionAddStatus::Committed:
                    line << "committed";
                    break;
                case CompletionAddStatus::Duplicate:
                    line << "duplicate";
                    break;
                case CompletionAddStatus::ReadOnly:
                    level = LogLevel::Warning;
                    line << "read-only";
                    break;
                case CompletionAddStatus::Failed:
                    level = LogLevel::Error;
                    line << "failed";
                    break;
            }
            line << " stable-id=" << candidate.record.stableId
                 << " stage=" << StageName(result.stage)
                 << " error=" << result.error;
            SafeLog(level, line.str());
        } catch (...) {
            SafeLog(LogLevel::Error, "completion-worker result-log-failed");
        }

        bool finish = false;
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            if (outstanding_ > 0u) {
                --outstanding_;
            }
            if (stopRequested_ && outstanding_ == 0u && queue_.empty()) {
                accepting_ = false;
                finished_ = true;
                finish = true;
            }
        } catch (...) {
            finish = true;
        }
        if (finish) {
            finishedCondition_.notify_all();
            return;
        }
    }
}

void CompletionWorker::SafeLog(LogLevel level, std::string message) noexcept {
    try {
        if (log_) {
            log_(level, std::move(message));
        }
    } catch (...) {
    }
}

}  // namespace campaign_completion
