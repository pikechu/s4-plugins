#pragma once

#include <windows.h>

#include <filesystem>
#include <mutex>
#include <string_view>

namespace campaign_completion {

bool IsCaptureTraceRootAdmitted(DWORD attributes,
                                bool finalPathAvailable) noexcept;

class CaptureTrace final {
public:
    bool Open(const std::filesystem::path& root, DWORD processId);
    bool Write(std::string_view record);
    void Close() noexcept;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
};

}  // namespace campaign_completion
