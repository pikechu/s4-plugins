#pragma once

#include "completion/CompletionFileOps.h"

namespace campaign_completion {

class Win32CompletionFileOps final : public ICompletionFileOps {
public:
    CompletionReadResult ReadBounded(
        const std::filesystem::path& path,
        std::size_t limit) noexcept override;
    CompletionWriteResult WriteTemporaryAndFlush(
        const std::filesystem::path& path,
        std::string_view bytes) noexcept override;
    CompletionWriteResult ReplaceWithBackup(
        const std::filesystem::path& main,
        const std::filesystem::path& temporary,
        const std::filesystem::path& backup) noexcept override;
    CompletionWriteResult MoveFirstWriteThrough(
        const std::filesystem::path& temporary,
        const std::filesystem::path& main) noexcept override;
};

}  // namespace campaign_completion
