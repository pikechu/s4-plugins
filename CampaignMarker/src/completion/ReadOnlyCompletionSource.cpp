#include "completion/ReadOnlyCompletionSource.h"

#include <utility>

namespace campaign_completion {

ReadOnlyCompletionSource::ReadOnlyCompletionSource(
    ICompletionFileOps& files, std::filesystem::path main) noexcept
    : files_(files), main_(std::move(main)) {}

ReadOnlyCompletionResult ReadOnlyCompletionSource::Load() noexcept {
    try {
        auto read = files_.ReadBounded(main_, kMaximumCompletionJsonBytes);
        if (read.status == CompletionFileStatus::Missing) {
            return {ReadOnlyCompletionStatus::Missing, {},
                    CompletionJsonFailure::None, read.error};
        }
        if (read.status != CompletionFileStatus::Success) {
            return {ReadOnlyCompletionStatus::Unavailable, {},
                    CompletionJsonFailure::None, read.error};
        }
        auto decoded = DecodeCompletionJson(read.bytes);
        if (!decoded) {
            return {ReadOnlyCompletionStatus::Malformed, {}, decoded.failure,
                    read.error};
        }
        return {ReadOnlyCompletionStatus::Loaded,
                std::move(decoded.snapshot), CompletionJsonFailure::None,
                read.error};
    } catch (...) {
        return {};
    }
}

}  // namespace campaign_completion
