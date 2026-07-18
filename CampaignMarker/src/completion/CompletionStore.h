#pragma once

#include "completion/CompletionFileOps.h"
#include "completion/CompletionJson.h"
#include "completion/ManualCompletionTransaction.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <set>

namespace campaign_completion {

enum class CompletionStoreMode {
    Uninitialized,
    WritableEmpty,
    WritableLoaded,
    ReadOnlyBackup,
    ReadOnlyNormalizationFailed,
    ReadOnlyUnavailable,
};

enum class CompletionAddStatus {
    Committed,
    Duplicate,
    ReadOnly,
    Failed,
};

struct CompletionLoadResult final {
    CompletionStoreMode mode = CompletionStoreMode::Uninitialized;
    CompletionJsonFailure failure = CompletionJsonFailure::None;
    DWORD error = ERROR_SUCCESS;
    std::size_t recordCount = 0u;
};

struct CompletionAddResult final {
    CompletionAddStatus status = CompletionAddStatus::Failed;
    CompletionTransactionStage stage = CompletionTransactionStage::None;
    DWORD error = ERROR_SUCCESS;
};

struct CompletionStoreSnapshot final {
    CompletionDatabaseSnapshot database;
    std::uint64_t revision = 0u;
    CompletionStoreMode mode = CompletionStoreMode::Uninitialized;
};

enum class ManualCompletionApplyStatus : std::uint8_t {
    Committed,
    Unchanged,
    Conflict,
    ReadOnly,
    Invalid,
    Failed,
};

struct ManualCompletionApplyResult final {
    ManualCompletionApplyStatus status =
        ManualCompletionApplyStatus::Failed;
    CompletionTransactionStage stage = CompletionTransactionStage::None;
    DWORD error = ERROR_SUCCESS;
    std::uint64_t revision = 0u;
};

class ICompletionStore {
public:
    virtual ~ICompletionStore() = default;
    virtual CompletionAddResult AddIfAbsent(
        const CompletionRecord& record) noexcept = 0;
    virtual CompletionDatabaseSnapshot Snapshot() const = 0;
};

class CompletionStore final : public ICompletionStore {
public:
    CompletionStore(ICompletionFileOps& files,
                    std::filesystem::path main,
                    std::filesystem::path temporary,
                    std::filesystem::path backup);
    CompletionLoadResult Load() noexcept;
    CompletionAddResult AddIfAbsent(
        const CompletionRecord& record) noexcept override;
    CompletionStoreMode Mode() const noexcept;
    CompletionDatabaseSnapshot Snapshot() const override;
    CompletionStoreSnapshot SnapshotWithRevision() const;
    ManualCompletionApplyResult ApplyManual(
        const ManualCompletionRequest& request) noexcept;

private:
    CompletionAddResult CommitSnapshot(
        CompletionDatabaseSnapshot candidate,
        std::set<std::string> candidateIds,
        bool replaceExisting) noexcept;
    void Publish(CompletionDatabaseSnapshot snapshot);

    ICompletionFileOps& files_;
    std::filesystem::path main_;
    std::filesystem::path temporary_;
    std::filesystem::path backup_;
    mutable std::mutex mutex_;
    CompletionDatabaseSnapshot snapshot_;
    std::set<std::string> stableIds_;
    CompletionStoreMode mode_ = CompletionStoreMode::Uninitialized;
    std::uint64_t revision_ = 0u;
};

}  // namespace campaign_completion
