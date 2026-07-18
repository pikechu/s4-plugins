#include "diagnostics/Phase3Trace.h"

#include "diagnostics/CaptureTrace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <string>

namespace campaign_completion {
namespace {

constexpr std::array<const wchar_t*, 5u> kFileNames{
    L"origin.trace", L"identity.trace", L"settlement-ui.trace",
    L"native-event.trace", L"decision.trace"};

constexpr std::array<std::string_view, 4u> kOriginPrefixes{
    "origin-source=", "origin-eligibility=", "origin-status=",
    "origin-refinement="};
constexpr std::array<std::string_view, 8u> kIdentityPrefixes{
    "lua-open-generation=", "map-init-session=", "su-map-name-status=",
    "su-map-relative-status=", "su-map-name=", "su-map-relative=",
    "identity-association=", "identity-status="};
constexpr std::array<std::string_view, 4u> kSettlementPrefixes{
    "settlement-capture=", "settlement-feature=", "local-player-status=",
    "local-player-lost="};
constexpr std::array<std::string_view, 4u> kNativeEventPrefixes{
    "native-subscription=", "native-event=", "native-event-duplicates=",
    "native-event-orphans="};
constexpr std::array<std::string_view, 2u> kDecisionPrefixes{
    "diagnostic-result=calibration-only", "controlled-stop-flush="};
constexpr std::array<std::string_view, 9u> kForbiddenTokens{
    "0x",          "chat=",       "player-name=",
    "completed_maps", "completion", "persistence",
    "marker",      "victory",     "module name="};

template <std::size_t Size>
bool HasPrefix(std::string_view record,
               const std::array<std::string_view, Size>& prefixes) noexcept {
    return std::any_of(prefixes.begin(), prefixes.end(),
                       [record](std::string_view prefix) {
                           return record.rfind(prefix, 0u) == 0u;
                       });
}

bool HasChannelPrefix(Phase3TraceChannel channel,
                      std::string_view record) noexcept {
    switch (channel) {
        case Phase3TraceChannel::Origin:
            return HasPrefix(record, kOriginPrefixes);
        case Phase3TraceChannel::Identity:
            return HasPrefix(record, kIdentityPrefixes);
        case Phase3TraceChannel::SettlementUi:
            return HasPrefix(record, kSettlementPrefixes);
        case Phase3TraceChannel::NativeEvent:
            return HasPrefix(record, kNativeEventPrefixes);
        case Phase3TraceChannel::Decision:
            return HasPrefix(record, kDecisionPrefixes);
        default:
            return false;
    }
}

bool IsAllowed(Phase3TraceChannel channel, std::string_view record) {
    if (record.empty() || record.size() > 1024u ||
        record.find_first_of("\r\n") != std::string_view::npos ||
        !HasChannelPrefix(channel, record) ||
        record.find(":\\") != std::string_view::npos ||
        record.find(":/") != std::string_view::npos ||
        record.find("\\\\") != std::string_view::npos) {
        return false;
    }

    std::string lowered(record);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return std::none_of(kForbiddenTokens.begin(), kForbiddenTokens.end(),
                        [&lowered](std::string_view token) {
                            return lowered.find(token) != std::string::npos;
                        });
}

std::filesystem::path CandidateDirectory(const std::filesystem::path& root,
                                         DWORD processId,
                                         unsigned suffix) {
    std::wstring name = L"phase-3-session-" + std::to_wstring(processId);
    if (suffix != 0u) {
        name += L"-" + std::to_wstring(suffix);
    }
    return root / name;
}

void CloseHandles(std::array<HANDLE, 5u>& handles) noexcept {
    for (auto& handle : handles) {
        if (handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(handle);
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
}

void RemoveCreatedFiles(const std::filesystem::path& directory,
                        std::size_t count) noexcept {
    try {
        for (std::size_t index = 0u; index < count; ++index) {
            const auto path = directory / kFileNames[index];
            DeleteFileW(path.c_str());
        }
        RemoveDirectoryW(directory.c_str());
    } catch (...) {
        // Cleanup is best-effort and must never escape a diagnostic failure.
    }
}

}  // namespace

bool Phase3Trace::Open(const std::filesystem::path& root, DWORD processId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handles_[0] != INVALID_HANDLE_VALUE || root.empty()) {
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
        if (!IsCaptureTraceRootAdmitted(attributes, finalPathAvailable)) {
            return false;
        }

        for (unsigned suffix = 0u; suffix <= 99u; ++suffix) {
            const auto candidate = CandidateDirectory(root, processId, suffix);
            if (CreateDirectoryW(candidate.c_str(), nullptr) == FALSE) {
                if (GetLastError() == ERROR_ALREADY_EXISTS) {
                    continue;
                }
                return false;
            }

            std::size_t created = 0u;
            for (; created < kFileNames.size(); ++created) {
                const auto path = candidate / kFileNames[created];
                handles_[created] = CreateFileW(
                    path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handles_[created] == INVALID_HANDLE_VALUE) {
                    break;
                }
            }
            if (created == kFileNames.size()) {
                directory_ = candidate;
                return true;
            }

            CloseHandles(handles_);
            RemoveCreatedFiles(candidate, created);
            return false;
        }
    } catch (...) {
        CloseHandles(handles_);
    }
    return false;
}

bool Phase3Trace::Write(Phase3TraceChannel channel,
                        std::string_view record) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto index = static_cast<std::size_t>(channel);
    if (index >= handles_.size() ||
        handles_[index] == INVALID_HANDLE_VALUE) {
        return false;
    }

    try {
        if (!IsAllowed(channel, record) ||
            record.size() >
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()) -
                    2u) {
            return false;
        }
        std::string line(record);
        line += "\r\n";
        DWORD written = 0u;
        const auto length = static_cast<DWORD>(line.size());
        return WriteFile(handles_[index], line.data(), length, &written,
                         nullptr) != FALSE &&
               written == length &&
               FlushFileBuffers(handles_[index]) != FALSE;
    } catch (...) {
        return false;
    }
}

void Phase3Trace::Close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    CloseHandles(handles_);
}

}  // namespace campaign_completion
