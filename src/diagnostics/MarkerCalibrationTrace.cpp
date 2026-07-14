#include "diagnostics/MarkerCalibrationTrace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace campaign_completion {
namespace {

constexpr std::array<std::string_view, 3> kAllowedPrefixes{
    "marker-state=", "marker-candidate=", "marker-frame="};

constexpr std::array<std::string_view, 8> kForbiddenTokens{
    "0x", "module name=", "base=", "memory",
    "pointer", "path-", ":\\", "\r"};

bool HasAllowedPrefix(std::string_view record) noexcept {
    return std::any_of(kAllowedPrefixes.begin(), kAllowedPrefixes.end(),
                       [record](std::string_view prefix) {
                           return record.rfind(prefix, 0u) == 0u;
                       });
}

std::filesystem::path CandidatePath(const std::filesystem::path& root,
                                    DWORD processId, unsigned suffix) {
    std::wstring name =
        L"marker-calibration-pid-" + std::to_wstring(processId);
    if (suffix != 0u) {
        name += L"-" + std::to_wstring(suffix);
    }
    name += L".trace";
    return root / name;
}

}  // namespace

bool IsMarkerCalibrationRecordAllowed(std::string_view record) noexcept {
    try {
        if (record.empty() ||
            record.size() > kMarkerCalibrationMaximumRecordBytes ||
            !HasAllowedPrefix(record) ||
            record.find_first_of("\r\n") != std::string_view::npos) {
            return false;
        }

        std::string lowered(record);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        return std::none_of(
            kForbiddenTokens.begin(), kForbiddenTokens.end(),
            [&lowered](std::string_view token) {
                return lowered.find(token) != std::string::npos;
            });
    } catch (...) {
        return false;
    }
}

bool IsMarkerCalibrationRootAdmitted(DWORD attributes,
                                     bool finalPathAvailable) noexcept {
    return attributes != INVALID_FILE_ATTRIBUTES && finalPathAvailable &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0u;
}

bool MarkerCalibrationTrace::Open(const std::filesystem::path& root,
                                  DWORD processId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE || root.empty()) {
        return false;
    }

    try {
        const DWORD attributes = GetFileAttributesW(root.c_str());
        HANDLE rootHandle = CreateFileW(
            root.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        const bool finalPathAvailable =
            rootHandle != INVALID_HANDLE_VALUE &&
            GetFinalPathNameByHandleW(rootHandle, nullptr, 0u,
                                      FILE_NAME_NORMALIZED) != 0u;
        if (rootHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(rootHandle);
        }
        if (!IsMarkerCalibrationRootAdmitted(attributes,
                                             finalPathAvailable)) {
            return false;
        }

        for (unsigned suffix = 0u; suffix <= 99u; ++suffix) {
            const auto candidate = CandidatePath(root, processId, suffix);
            HANDLE candidateHandle = CreateFileW(
                candidate.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (candidateHandle != INVALID_HANDLE_VALUE) {
                handle_ = candidateHandle;
                path_ = candidate;
                bytesWritten_ = 0u;
                return true;
            }
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool MarkerCalibrationTrace::Write(std::string_view record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ == INVALID_HANDLE_VALUE ||
        !IsMarkerCalibrationRecordAllowed(record)) {
        return false;
    }

    try {
        std::string line(record);
        line += "\r\n";
        if (line.size() > kMarkerCalibrationMaximumBytes - bytesWritten_) {
            return false;
        }

        DWORD written = 0u;
        const auto length = static_cast<DWORD>(line.size());
        if (WriteFile(handle_, line.data(), length, &written, nullptr) == FALSE ||
            written != length || FlushFileBuffers(handle_) == FALSE) {
            return false;
        }
        bytesWritten_ += written;
        return true;
    } catch (...) {
        return false;
    }
}

void MarkerCalibrationTrace::Close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(handle_);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

}  // namespace campaign_completion
