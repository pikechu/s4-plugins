#include "completion/CompletionStore.h"

#include <utility>

namespace campaign_completion {
namespace {

CompletionLoadResult LoadResult(CompletionStoreMode mode,
                                CompletionJsonFailure failure,
                                DWORD error,
                                std::size_t count) noexcept {
    return {mode, failure, error, count};
}

CompletionAddResult AddFailure(CompletionTransactionStage stage,
                               DWORD error) noexcept {
    return {CompletionAddStatus::Failed, stage, error};
}

}  // namespace

CompletionStore::CompletionStore(ICompletionFileOps& files,
                                 std::filesystem::path main,
                                 std::filesystem::path temporary,
                                 std::filesystem::path backup)
    : files_(files),
      main_(std::move(main)),
      temporary_(std::move(temporary)),
      backup_(std::move(backup)) {}

CompletionLoadResult CompletionStore::Load() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mode_ != CompletionStoreMode::Uninitialized) {
        return LoadResult(mode_, CompletionJsonFailure::None, ERROR_SUCCESS,
                          snapshot_.records.size());
    }
    try {
        const auto mainRead =
            files_.ReadBounded(main_, kMaximumCompletionJsonBytes);
        CompletionJsonResult mainJson{};
        if (mainRead.status == CompletionFileStatus::Success) {
            mainJson = DecodeCompletionJson(mainRead.bytes);
            if (mainJson) {
                Publish(std::move(mainJson.snapshot));
                mode_ = CompletionStoreMode::WritableLoaded;
                return LoadResult(mode_, CompletionJsonFailure::None,
                                  ERROR_SUCCESS, snapshot_.records.size());
            }
        }

        const auto backupRead =
            files_.ReadBounded(backup_, kMaximumCompletionJsonBytes);
        CompletionJsonResult backupJson{};
        if (backupRead.status == CompletionFileStatus::Success) {
            backupJson = DecodeCompletionJson(backupRead.bytes);
            if (backupJson) {
                Publish(std::move(backupJson.snapshot));
                mode_ = CompletionStoreMode::ReadOnlyBackup;
                const DWORD error =
                    mainRead.status == CompletionFileStatus::Failed
                        ? mainRead.error
                        : ERROR_SUCCESS;
                const auto failure =
                    mainRead.status == CompletionFileStatus::Success
                        ? mainJson.failure
                        : CompletionJsonFailure::None;
                return LoadResult(mode_, failure, error,
                                  snapshot_.records.size());
            }
        }

        if (mainRead.status == CompletionFileStatus::Missing &&
            backupRead.status == CompletionFileStatus::Missing) {
            mode_ = CompletionStoreMode::WritableEmpty;
            return LoadResult(mode_, CompletionJsonFailure::None,
                              ERROR_SUCCESS, 0u);
        }

        mode_ = CompletionStoreMode::ReadOnlyUnavailable;
        const DWORD error =
            mainRead.status == CompletionFileStatus::Failed
                ? mainRead.error
                : (backupRead.status == CompletionFileStatus::Failed
                       ? backupRead.error
                       : ERROR_SUCCESS);
        const auto failure =
            mainRead.status == CompletionFileStatus::Success
                ? mainJson.failure
                : (backupRead.status == CompletionFileStatus::Success
                       ? backupJson.failure
                       : CompletionJsonFailure::None);
        return LoadResult(mode_, failure, error, 0u);
    } catch (...) {
        snapshot_ = {};
        stableIds_.clear();
        mode_ = CompletionStoreMode::ReadOnlyUnavailable;
        return LoadResult(mode_, CompletionJsonFailure::None,
                          ERROR_OUTOFMEMORY, 0u);
    }
}

CompletionAddResult CompletionStore::AddIfAbsent(
    const CompletionRecord& record) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        if (mode_ != CompletionStoreMode::WritableEmpty &&
            mode_ != CompletionStoreMode::WritableLoaded) {
            return {CompletionAddStatus::ReadOnly,
                    CompletionTransactionStage::None, ERROR_ACCESS_DENIED};
        }
        if (stableIds_.find(record.stableId) != stableIds_.end()) {
            return {CompletionAddStatus::Duplicate,
                    CompletionTransactionStage::None, ERROR_SUCCESS};
        }

        CompletionDatabaseSnapshot candidate = snapshot_;
        candidate.records.push_back(record);
        const auto encoded = EncodeCompletionJson(candidate);
        if (!encoded.has_value()) {
            return AddFailure(CompletionTransactionStage::Encode,
                              ERROR_INVALID_DATA);
        }
        const auto write =
            files_.WriteTemporaryAndFlush(temporary_, *encoded);
        if (!write.success) {
            return AddFailure(write.stage, write.error);
        }
        const auto temporaryRead =
            files_.ReadBounded(temporary_, kMaximumCompletionJsonBytes);
        if (temporaryRead.status != CompletionFileStatus::Success) {
            return AddFailure(CompletionTransactionStage::ReadTemporary,
                              temporaryRead.error);
        }
        auto verified = DecodeCompletionJson(temporaryRead.bytes);
        if (!verified) {
            return AddFailure(CompletionTransactionStage::ValidateTemporary,
                              ERROR_INVALID_DATA);
        }
        const auto verifiedBytes = EncodeCompletionJson(verified.snapshot);
        if (!verifiedBytes.has_value() || *verifiedBytes != *encoded) {
            return AddFailure(CompletionTransactionStage::ValidateTemporary,
                              ERROR_INVALID_DATA);
        }

        CompletionWriteResult commit{};
        if (mode_ == CompletionStoreMode::WritableLoaded) {
            commit = files_.ReplaceWithBackup(main_, temporary_, backup_);
        } else {
            commit = files_.MoveFirstWriteThrough(temporary_, main_);
        }
        if (!commit.success) {
            return AddFailure(commit.stage, commit.error);
        }
        Publish(std::move(verified.snapshot));
        mode_ = CompletionStoreMode::WritableLoaded;
        return {CompletionAddStatus::Committed,
                CompletionTransactionStage::None, ERROR_SUCCESS};
    } catch (...) {
        return AddFailure(CompletionTransactionStage::Encode,
                          ERROR_OUTOFMEMORY);
    }
}

CompletionStoreMode CompletionStore::Mode() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

CompletionDatabaseSnapshot CompletionStore::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void CompletionStore::Publish(CompletionDatabaseSnapshot snapshot) {
    std::set<std::string> ids;
    for (const auto& record : snapshot.records) {
        ids.insert(record.stableId);
    }
    snapshot_ = std::move(snapshot);
    stableIds_ = std::move(ids);
}

}  // namespace campaign_completion
