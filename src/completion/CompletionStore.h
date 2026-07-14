#pragma once

#include "completion/CompletionFileOps.h"
#include "completion/CompletionJson.h"

#include <filesystem>
#include <mutex>
#include <set>

namespace campaign_completion {

enum class CompletionStoreMode {
    Uninitialized,
    WritableEmpty,
    WritableLoaded,
    ReadOnlyBackup,
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

class ICompletionStore {
public:
    virtual ~ICompletionStore() = default;
    virtual CompletionAddResult AddIfAbsent(
        const CompletionRecord& record) noexcept = 0;
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
    CompletionDatabaseSnapshot Snapshot() const;

private:
    void Publish(CompletionDatabaseSnapshot snapshot);

    ICompletionFileOps& files_;
    std::filesystem::path main_;
    std::filesystem::path temporary_;
    std::filesystem::path backup_;
    mutable std::mutex mutex_;
    CompletionDatabaseSnapshot snapshot_;
    std::set<std::string> stableIds_;
    CompletionStoreMode mode_ = CompletionStoreMode::Uninitialized;
};

}  // namespace campaign_completion
