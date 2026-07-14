#include "completion/Win32CompletionFileOps.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace campaign_completion {
namespace {

CompletionWriteResult WriteFailure(CompletionTransactionStage stage,
                                   DWORD error) noexcept {
    return {false, stage, error};
}

}  // namespace

CompletionReadResult Win32CompletionFileOps::ReadBounded(
    const std::filesystem::path& path,
    std::size_t limit) noexcept {
    const HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return {CompletionFileStatus::Missing, {}, error};
        }
        return {CompletionFileStatus::Failed, {}, error};
    }
    try {
        LARGE_INTEGER size{};
        if (GetFileSizeEx(handle, &size) == FALSE || size.QuadPart < 0) {
            const DWORD error = GetLastError();
            CloseHandle(handle);
            return {CompletionFileStatus::Failed, {}, error};
        }
        const auto unsignedSize = static_cast<unsigned long long>(size.QuadPart);
        if (unsignedSize > static_cast<unsigned long long>(limit) ||
            unsignedSize > static_cast<unsigned long long>(
                               (std::numeric_limits<std::size_t>::max)())) {
            CloseHandle(handle);
            return {CompletionFileStatus::Failed, {}, ERROR_FILE_TOO_LARGE};
        }
        std::string bytes(static_cast<std::size_t>(unsignedSize), '\0');
        std::size_t offset = 0u;
        while (offset < bytes.size()) {
            const std::size_t remaining = bytes.size() - offset;
            const DWORD requested = static_cast<DWORD>((std::min)(
                remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD received = 0u;
            if (ReadFile(handle, bytes.data() + offset, requested, &received,
                         nullptr) == FALSE || received == 0u) {
                const DWORD error = GetLastError();
                CloseHandle(handle);
                return {CompletionFileStatus::Failed, {},
                        error == ERROR_SUCCESS ? ERROR_HANDLE_EOF : error};
            }
            offset += received;
        }
        if (CloseHandle(handle) == FALSE) {
            return {CompletionFileStatus::Failed, {}, GetLastError()};
        }
        return {CompletionFileStatus::Success, std::move(bytes),
                ERROR_SUCCESS};
    } catch (...) {
        CloseHandle(handle);
        return {CompletionFileStatus::Failed, {}, ERROR_OUTOFMEMORY};
    }
}

CompletionWriteResult Win32CompletionFileOps::WriteTemporaryAndFlush(
    const std::filesystem::path& path,
    std::string_view bytes) noexcept {
    const HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return WriteFailure(CompletionTransactionStage::WriteTemporary,
                            GetLastError());
    }
    std::size_t offset = 0u;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD requested = static_cast<DWORD>((std::min)(
            remaining,
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0u;
        if (WriteFile(handle, bytes.data() + offset, requested, &written,
                      nullptr) == FALSE || written == 0u) {
            const DWORD error = GetLastError();
            CloseHandle(handle);
            return WriteFailure(CompletionTransactionStage::WriteTemporary,
                                error == ERROR_SUCCESS ? ERROR_WRITE_FAULT
                                                       : error);
        }
        offset += written;
    }
    if (FlushFileBuffers(handle) == FALSE) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return WriteFailure(CompletionTransactionStage::FlushTemporary,
                            error);
    }
    if (CloseHandle(handle) == FALSE) {
        return WriteFailure(CompletionTransactionStage::FlushTemporary,
                            GetLastError());
    }
    return {true, CompletionTransactionStage::None, ERROR_SUCCESS};
}

CompletionWriteResult Win32CompletionFileOps::ReplaceWithBackup(
    const std::filesystem::path& main,
    const std::filesystem::path& temporary,
    const std::filesystem::path& backup) noexcept {
    if (ReplaceFileW(main.c_str(), temporary.c_str(), backup.c_str(), 0u,
                     nullptr, nullptr) == FALSE) {
        return WriteFailure(CompletionTransactionStage::ReplaceMain,
                            GetLastError());
    }
    return {true, CompletionTransactionStage::None, ERROR_SUCCESS};
}

CompletionWriteResult Win32CompletionFileOps::MoveFirstWriteThrough(
    const std::filesystem::path& temporary,
    const std::filesystem::path& main) noexcept {
    if (MoveFileExW(temporary.c_str(), main.c_str(), MOVEFILE_WRITE_THROUGH) ==
        FALSE) {
        return WriteFailure(CompletionTransactionStage::MoveFirst,
                            GetLastError());
    }
    return {true, CompletionTransactionStage::None, ERROR_SUCCESS};
}

}  // namespace campaign_completion
