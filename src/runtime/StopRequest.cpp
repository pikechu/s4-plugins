#include "runtime/StopRequest.h"

#include <system_error>

namespace campaign_completion {

bool ConsumeStopRequest(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || !exists) {
        return false;
    }
    const bool removed = std::filesystem::remove(path, error);
    return removed && !error;
}

}  // namespace campaign_completion
