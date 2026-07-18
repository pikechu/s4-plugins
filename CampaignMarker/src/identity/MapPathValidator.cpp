#include "identity/MapPathValidator.h"

#include <windows.h>

#include <algorithm>
#include <climits>
#include <cwctype>
#include <limits>
#include <utility>
#include <vector>

namespace campaign_completion {
namespace {

class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE value = INVALID_HANDLE_VALUE) noexcept
        : value_(value) {}

    ~UniqueHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    HANDLE Get() const noexcept {
        return value_;
    }

    explicit operator bool() const noexcept {
        return value_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE value_;
};

MapPathResult Failed(MapPathFailure failure) {
    MapPathResult result{};
    result.failure = failure;
    return result;
}

bool EqualOrdinalIgnoreCase(std::wstring_view left,
                            std::wstring_view right) noexcept {
    if (left.size() != right.size() ||
        left.size() > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                                right.data(), static_cast<int>(right.size()),
                                TRUE) == CSTR_EQUAL;
}

bool ContainsControlCharacter(std::wstring_view value) noexcept {
    return std::any_of(value.begin(), value.end(), [](wchar_t character) {
        const auto code = static_cast<unsigned int>(character);
        return code < 0x20u || (code >= 0x7Fu && code <= 0x9Fu);
    });
}

std::vector<std::wstring_view> Components(std::wstring_view path) {
    std::vector<std::wstring_view> result;
    std::size_t start = 0;
    for (;;) {
        const auto separator = path.find(L'\\', start);
        const auto end = separator == std::wstring_view::npos
                             ? path.size()
                             : separator;
        result.push_back(path.substr(start, end - start));
        if (separator == std::wstring_view::npos) {
            return result;
        }
        start = separator + 1u;
    }
}

bool HasMapExtension(std::wstring_view component) noexcept {
    constexpr std::wstring_view extension = L".map";
    return component.size() >= extension.size() &&
           EqualOrdinalIgnoreCase(
               component.substr(component.size() - extension.size()),
               extension);
}

UniqueHandle OpenForFinalPath(const std::filesystem::path& path,
                              bool directory) noexcept {
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (directory) {
        flags |= FILE_FLAG_BACKUP_SEMANTICS;
    }
    return UniqueHandle(CreateFileW(
        path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, flags, nullptr));
}

std::wstring FinalPath(HANDLE handle) {
    constexpr DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const DWORD required = GetFinalPathNameByHandleW(handle, nullptr, 0u, flags);
    if (required == 0u || required == (std::numeric_limits<DWORD>::max)()) {
        return {};
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1u);
    const DWORD written = GetFinalPathNameByHandleW(
        handle, buffer.data(), static_cast<DWORD>(buffer.size()), flags);
    if (written == 0u || written >= buffer.size()) {
        return {};
    }
    return std::wstring(buffer.data(), written);
}

std::string ToUtf8(std::wstring_view value) {
    if (value.size() > static_cast<std::size_t>(INT_MAX)) {
        return {};
    }
    const int sourceLength = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), sourceLength, nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            sourceLength, result.data(), required, nullptr,
                            nullptr) != required) {
        return {};
    }
    return result;
}

}  // namespace

bool IsFinalPathBeneath(std::wstring_view root,
                        std::wstring_view candidate) noexcept {
    while (!root.empty() &&
           (root.back() == L'\\' || root.back() == L'/')) {
        root.remove_suffix(1u);
    }
    if (root.empty() || candidate.size() <= root.size() ||
        root.size() > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    if (CompareStringOrdinal(root.data(), static_cast<int>(root.size()),
                             candidate.data(), static_cast<int>(root.size()),
                             TRUE) != CSTR_EQUAL) {
        return false;
    }
    return candidate[root.size()] == L'\\' || candidate[root.size()] == L'/';
}

MapPathValidator::MapPathValidator(std::filesystem::path gameRoot)
    : gameRoot_(std::move(gameRoot)) {}

MapPathResult MapPathValidator::ValidateWide(std::wstring_view raw) const {
    if (raw.empty()) {
        return Failed(MapPathFailure::Empty);
    }
    if (ContainsControlCharacter(raw)) {
        return Failed(MapPathFailure::ControlCharacter);
    }

    std::wstring normalized(raw);
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    if (normalized.front() == L'\\' || normalized.find(L':') != std::wstring::npos) {
        return Failed(MapPathFailure::RootedPath);
    }

    const auto components = Components(normalized);
    if (components.empty()) {
        return Failed(MapPathFailure::Empty);
    }
    if (std::any_of(components.begin(), components.end(),
                    [](std::wstring_view component) {
                        return component.empty() || component == L"." ||
                               component == L"..";
                    })) {
        return Failed(MapPathFailure::Traversal);
    }
    if (!EqualOrdinalIgnoreCase(components.front(), L"Map")) {
        return Failed(MapPathFailure::WrongRoot);
    }
    if (!HasMapExtension(components.back())) {
        return Failed(MapPathFailure::WrongExtension);
    }

    const auto mapRootPath = gameRoot_ / L"Map";
    const auto candidatePath = gameRoot_ / std::filesystem::path(normalized);
    const auto mapRoot = OpenForFinalPath(mapRootPath, true);
    if (!mapRoot) {
        return Failed(MapPathFailure::MapRootUnavailable);
    }
    const auto candidate = OpenForFinalPath(candidatePath, true);
    if (!candidate) {
        return Failed(MapPathFailure::CandidateUnavailable);
    }

    BY_HANDLE_FILE_INFORMATION information{};
    if (GetFileInformationByHandle(candidate.Get(), &information) == FALSE) {
        return Failed(MapPathFailure::CandidateUnavailable);
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return Failed(MapPathFailure::NotRegularFile);
    }

    const auto finalRoot = FinalPath(mapRoot.Get());
    const auto finalCandidate = FinalPath(candidate.Get());
    if (finalRoot.empty() || finalCandidate.empty()) {
        return Failed(MapPathFailure::FinalPathUnavailable);
    }
    if (!IsFinalPathBeneath(finalRoot, finalCandidate)) {
        return Failed(MapPathFailure::OutsideMapRoot);
    }

    auto utf8 = ToUtf8(normalized);
    if (utf8.empty()) {
        return Failed(MapPathFailure::Utf8ConversionFailed);
    }
    return {std::move(utf8), MapPathFailure::None};
}

}  // namespace campaign_completion
