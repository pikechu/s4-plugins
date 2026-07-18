#pragma once

#include "completion/CompletionFileOps.h"
#include "completion/CompletionJson.h"

#include <filesystem>

namespace campaign_completion {

enum class ReadOnlyCompletionStatus {
    Loaded,
    Missing,
    Malformed,
    Unavailable,
};

struct ReadOnlyCompletionResult final {
    ReadOnlyCompletionStatus status = ReadOnlyCompletionStatus::Unavailable;
    CompletionDatabaseSnapshot snapshot;
    CompletionJsonFailure failure = CompletionJsonFailure::None;
    DWORD error = ERROR_SUCCESS;
};

class ReadOnlyCompletionSource final {
public:
    ReadOnlyCompletionSource(ICompletionFileOps& files,
                             std::filesystem::path main) noexcept;
    ReadOnlyCompletionResult Load() noexcept;

private:
    ICompletionFileOps& files_;
    std::filesystem::path main_;
};

}  // namespace campaign_completion
