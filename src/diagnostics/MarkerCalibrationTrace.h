#pragma once

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace campaign_completion {

inline constexpr std::size_t kMarkerCalibrationMaximumBytes = 256u * 1024u;
inline constexpr std::size_t kMarkerCalibrationMaximumRecordBytes = 2048u;

bool IsMarkerCalibrationRecordAllowed(std::string_view record) noexcept;
bool IsMarkerCalibrationRootAdmitted(DWORD attributes,
                                     bool finalPathAvailable) noexcept;

class MarkerCalibrationTrace final {
public:
    bool Open(const std::filesystem::path& root, DWORD processId);
    bool Write(std::string_view record);
    void Close() noexcept;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
    std::size_t bytesWritten_ = 0u;
};

}  // namespace campaign_completion
