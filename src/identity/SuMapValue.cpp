#include "identity/SuMapValue.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace campaign_completion {
namespace {

constexpr std::size_t kMaximumSuMapValueBytes = 512u;

SuMapReadStatus ValidateBytes(std::string_view bytes) noexcept {
    if (bytes.empty()) {
        return SuMapReadStatus::Empty;
    }
    if (bytes.size() > kMaximumSuMapValueBytes) {
        return SuMapReadStatus::TooLong;
    }
    const auto control = std::find_if(
        bytes.begin(), bytes.end(), [](char value) {
            const auto byte = static_cast<unsigned char>(value);
            return byte < 0x20u || byte == 0x7fu;
        });
    return control == bytes.end() ? SuMapReadStatus::Success
                                  : SuMapReadStatus::ControlCharacter;
}

bool ToWide(std::string_view bytes, std::wstring& result) {
    const int required = MultiByteToWideChar(
        CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (required <= 0) {
        return false;
    }
    result.assign(static_cast<std::size_t>(required), L'\0');
    return MultiByteToWideChar(CP_ACP, 0, bytes.data(),
                               static_cast<int>(bytes.size()), result.data(),
                               required) == required;
}

bool ToUtf8(std::wstring_view value, std::string& result) {
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return false;
    }
    result.assign(static_cast<std::size_t>(required), '\0');
    return WideCharToMultiByte(CP_UTF8, 0, value.data(),
                               static_cast<int>(value.size()), result.data(),
                               required, nullptr, nullptr) == required;
}

ValidatedSuValue ConvertValidated(std::string_view bytes) {
    ValidatedSuValue result;
    result.status = ValidateBytes(bytes);
    if (result.status != SuMapReadStatus::Success) {
        return result;
    }
    if (!ToWide(bytes, result.normalized) ||
        !ToUtf8(result.normalized, result.displayUtf8)) {
        result.status = SuMapReadStatus::EncodingFailure;
        result.normalized.clear();
        result.displayUtf8.clear();
        return result;
    }
    result.status = SuMapReadStatus::Success;
    return result;
}

bool IsAbsoluteOrQualified(std::wstring_view value) noexcept {
    if (value.empty()) {
        return false;
    }
    if (value.front() == L'\\' || value.front() == L'/') {
        return true;
    }
    if (value.size() >= 2u && value[1] == L':') {
        return true;
    }
    return value.find(L':') != std::wstring_view::npos;
}

}  // namespace

ValidatedSuValue ValidateSuMapName(std::string_view bytes) {
    return ConvertValidated(bytes);
}

ValidatedSuValue ValidateSuRelativePath(std::string_view bytes) {
    auto result = ConvertValidated(bytes);
    if (!result) {
        return result;
    }
    if (IsAbsoluteOrQualified(result.normalized)) {
        result.status = SuMapReadStatus::AbsolutePath;
        result.normalized.clear();
        result.displayUtf8.clear();
        return result;
    }

    std::replace(result.normalized.begin(), result.normalized.end(), L'/', L'\\');
    std::vector<std::wstring> components;
    std::size_t start = 0u;
    while (start <= result.normalized.size()) {
        const auto end = result.normalized.find(L'\\', start);
        const auto count = end == std::wstring::npos
                               ? result.normalized.size() - start
                               : end - start;
        const auto component = result.normalized.substr(start, count);
        if (component == L"..") {
            result.status = SuMapReadStatus::PathTraversal;
            result.normalized.clear();
            result.displayUtf8.clear();
            return result;
        }
        if (!component.empty() && component != L".") {
            components.push_back(component);
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1u;
    }
    if (components.empty()) {
        result.status = SuMapReadStatus::Empty;
        result.normalized.clear();
        result.displayUtf8.clear();
        return result;
    }

    result.normalized.clear();
    for (std::size_t index = 0u; index < components.size(); ++index) {
        if (index != 0u) {
            result.normalized.push_back(L'\\');
        }
        result.normalized += components[index];
    }
    if (!ToUtf8(result.normalized, result.displayUtf8)) {
        result.status = SuMapReadStatus::EncodingFailure;
        result.normalized.clear();
        result.displayUtf8.clear();
        return result;
    }
    result.status = SuMapReadStatus::Success;
    return result;
}

std::string_view SuMapReadStatusName(SuMapReadStatus status) noexcept {
    switch (status) {
        case SuMapReadStatus::Success:
            return "success";
        case SuMapReadStatus::SuTableMissing:
            return "su-table-missing";
        case SuMapReadStatus::GameTableMissing:
            return "game-table-missing";
        case SuMapReadStatus::FunctionMissing:
            return "function-missing";
        case SuMapReadStatus::CallError:
            return "call-error";
        case SuMapReadStatus::NonString:
            return "non-string";
        case SuMapReadStatus::Empty:
            return "empty";
        case SuMapReadStatus::TooLong:
            return "too-long";
        case SuMapReadStatus::ControlCharacter:
            return "control-character";
        case SuMapReadStatus::EncodingFailure:
            return "encoding-failure";
        case SuMapReadStatus::AbsolutePath:
            return "absolute-path";
        case SuMapReadStatus::PathTraversal:
            return "path-traversal";
        case SuMapReadStatus::ValueConflict:
            return "value-conflict";
        case SuMapReadStatus::StaleGeneration:
            return "stale-generation";
        case SuMapReadStatus::Timeout:
            return "timeout";
    }
    return "unknown";
}

}  // namespace campaign_completion
