#pragma once

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace campaign_completion {

enum class CompletionFileStatus {
    Success,
    Missing,
    Failed,
};

enum class CompletionTransactionStage {
    None,
    ReadMain,
    ReadBackup,
    Encode,
    WriteTemporary,
    FlushTemporary,
    ReadTemporary,
    ValidateTemporary,
    BackupMain,
    ReplaceMain,
    MoveFirst,
};

struct CompletionReadResult final {
    CompletionFileStatus status = CompletionFileStatus::Failed;
    std::string bytes;
    DWORD error = ERROR_SUCCESS;
};

struct CompletionWriteResult final {
    bool success = false;
    CompletionTransactionStage stage = CompletionTransactionStage::None;
    DWORD error = ERROR_SUCCESS;
};

class ICompletionFileOps {
public:
    virtual ~ICompletionFileOps() = default;
    virtual CompletionReadResult ReadBounded(
        const std::filesystem::path& path,
        std::size_t limit) noexcept = 0;
    virtual CompletionWriteResult WriteTemporaryAndFlush(
        const std::filesystem::path& path,
        std::string_view bytes) noexcept = 0;
    virtual CompletionWriteResult ReplaceWithBackup(
        const std::filesystem::path& main,
        const std::filesystem::path& temporary,
        const std::filesystem::path& backup) noexcept = 0;
    virtual CompletionWriteResult MoveFirstWriteThrough(
        const std::filesystem::path& temporary,
        const std::filesystem::path& main) noexcept = 0;
};

}  // namespace campaign_completion
