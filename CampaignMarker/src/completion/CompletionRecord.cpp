#include "completion/CompletionRecord.h"

#include "victory/MapSessionPolicy.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

namespace campaign_completion {
namespace {

bool IsValidUtf16(std::wstring_view value) noexcept {
    for (std::size_t index = 0u; index < value.size(); ++index) {
        const auto unit = static_cast<unsigned>(value[index]);
        if (unit == 0u) {
            return false;
        }
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (index + 1u >= value.size()) {
                return false;
            }
            const auto next = static_cast<unsigned>(value[index + 1u]);
            if (next < 0xDC00u || next > 0xDFFFu) {
                return false;
            }
            ++index;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

bool HasInvalidComponent(std::wstring_view value) noexcept {
    std::size_t start = 0u;
    while (start <= value.size()) {
        const auto end = value.find(L'\\', start);
        const auto count = end == std::wstring_view::npos
                               ? value.size() - start
                               : end - start;
        const auto component = value.substr(start, count);
        if (component.empty() || component == L"." || component == L"..") {
            return true;
        }
        if (end == std::wstring_view::npos) {
            return false;
        }
        start = end + 1u;
    }
    return false;
}

bool SameUtcFields(const SYSTEMTIME& left,
                   const SYSTEMTIME& right) noexcept {
    return left.wYear == right.wYear && left.wMonth == right.wMonth &&
           left.wDay == right.wDay && left.wHour == right.wHour &&
           left.wMinute == right.wMinute && left.wSecond == right.wSecond;
}

bool IsDigit(char value) noexcept {
    return value >= '0' && value <= '9';
}

unsigned ParseDecimal(std::string_view value,
                      std::size_t offset,
                      std::size_t count) noexcept {
    unsigned result = 0u;
    for (std::size_t index = 0u; index < count; ++index) {
        const char digit = value[offset + index];
        if (!IsDigit(digit)) {
            return (std::numeric_limits<unsigned>::max)();
        }
        result = result * 10u + static_cast<unsigned>(digit - '0');
    }
    return result;
}

}  // namespace

std::optional<std::wstring> StrictUtf8ToWide(
    std::string_view value) noexcept {
    if (value.size() >
        static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }
    if (value.empty()) {
        return std::wstring{};
    }
    const int length = static_cast<int>(value.size());
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), length, nullptr, 0);
    if (required <= 0) {
        return std::nullopt;
    }
    try {
        std::wstring result(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                value.data(), length, result.data(),
                                required) != required) {
            return std::nullopt;
        }
        if (std::find(result.begin(), result.end(), L'\0') != result.end()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> WideToStrictUtf8(
    std::wstring_view value) noexcept {
    if (!IsValidUtf16(value) ||
        value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }
    if (value.empty()) {
        return std::string{};
    }
    const int length = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return std::nullopt;
    }
    try {
        std::string result(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                length, result.data(), required, nullptr,
                                nullptr) != required) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> BuildStableMapId(
    std::wstring_view relative) noexcept {
    if (relative.empty() || relative.size() > kMaximumRelativeIdentityUnits ||
        !IsValidUtf16(relative) || relative.front() == L'\\' ||
        relative.front() == L'/' ||
        (relative.size() >= 2u && relative[1] == L':')) {
        return std::nullopt;
    }
    try {
        std::wstring normalized(relative);
        for (auto& unit : normalized) {
            if (unit == L'/') {
                unit = L'\\';
            }
        }
        if (HasInvalidComponent(normalized) ||
            normalized.size() >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return std::nullopt;
        }
        const int sourceLength = static_cast<int>(normalized.size());
        const int required = LCMapStringEx(
            LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, normalized.data(),
            sourceLength, nullptr, 0, nullptr, nullptr, 0);
        if (required <= 0 ||
            static_cast<std::size_t>(required) >
                kMaximumCompletionStringUnits) {
            return std::nullopt;
        }
        std::wstring lowered(static_cast<std::size_t>(required), L'\0');
        if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                          normalized.data(), sourceLength, lowered.data(),
                          required, nullptr, nullptr, 0) != required ||
            !IsValidUtf16(lowered)) {
            return std::nullopt;
        }
        const auto utf8 = WideToStrictUtf8(lowered);
        if (!utf8.has_value()) {
            return std::nullopt;
        }
        return std::string("map:") + *utf8;
    } catch (...) {
        return std::nullopt;
    }
}

CompletionMapKind CompletionKindFor(std::wstring_view relative) noexcept {
    return IsRandomMapIdentifier(relative) ? CompletionMapKind::Random
                                           : CompletionMapKind::Fixed;
}

std::optional<std::string> FormatUtcCompletionTime(
    const SYSTEMTIME& utc) noexcept {
    FILETIME fileTime{};
    if (SystemTimeToFileTime(&utc, &fileTime) == FALSE) {
        return std::nullopt;
    }
    SYSTEMTIME roundTrip{};
    if (FileTimeToSystemTime(&fileTime, &roundTrip) == FALSE ||
        !SameUtcFields(utc, roundTrip)) {
        return std::nullopt;
    }
    std::array<char, 21> output{};
    const int written = std::snprintf(
        output.data(), output.size(), "%04u-%02u-%02uT%02u:%02u:%02uZ",
        static_cast<unsigned>(utc.wYear),
        static_cast<unsigned>(utc.wMonth),
        static_cast<unsigned>(utc.wDay),
        static_cast<unsigned>(utc.wHour),
        static_cast<unsigned>(utc.wMinute),
        static_cast<unsigned>(utc.wSecond));
    if (written != 20) {
        return std::nullopt;
    }
    return std::string(output.data(), 20u);
}

bool IsValidUtcCompletionTime(std::string_view value) noexcept {
    if (value.size() != 20u || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
        value[19] != 'Z') {
        return false;
    }
    const auto year = ParseDecimal(value, 0u, 4u);
    const auto month = ParseDecimal(value, 5u, 2u);
    const auto day = ParseDecimal(value, 8u, 2u);
    const auto hour = ParseDecimal(value, 11u, 2u);
    const auto minute = ParseDecimal(value, 14u, 2u);
    const auto second = ParseDecimal(value, 17u, 2u);
    if (year > (std::numeric_limits<WORD>::max)() ||
        month > (std::numeric_limits<WORD>::max)() ||
        day > (std::numeric_limits<WORD>::max)() ||
        hour > (std::numeric_limits<WORD>::max)() ||
        minute > (std::numeric_limits<WORD>::max)() ||
        second > (std::numeric_limits<WORD>::max)()) {
        return false;
    }
    SYSTEMTIME parsed{};
    parsed.wYear = static_cast<WORD>(year);
    parsed.wMonth = static_cast<WORD>(month);
    parsed.wDay = static_cast<WORD>(day);
    parsed.wHour = static_cast<WORD>(hour);
    parsed.wMinute = static_cast<WORD>(minute);
    parsed.wSecond = static_cast<WORD>(second);
    const auto formatted = FormatUtcCompletionTime(parsed);
    return formatted.has_value() && *formatted == value;
}

std::string CurrentUtcCompletionTime() noexcept {
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    const auto formatted = FormatUtcCompletionTime(utc);
    return formatted.value_or(std::string{});
}

}  // namespace campaign_completion
