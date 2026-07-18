#include "marker/BoundedMenuText.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace campaign_completion {
namespace {

bool CopyGuiTextBytes(const char* text, char* destination,
                      std::uint16_t* length) noexcept {
    if (text == nullptr || destination == nullptr || length == nullptr) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0u; index < kMaximumMarkerTextUnits;
             ++index) {
            const char value = text[index];
            destination[index] = value;
            if (value == '\0') {
                *length = static_cast<std::uint16_t>(index);
                return true;
            }
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return false;
}

bool IsControl(wchar_t unit) noexcept {
    const auto value = static_cast<unsigned>(unit);
    return value <= 0x1Fu || (value >= 0x7Fu && value <= 0x9Fu);
}

bool IsSpace(wchar_t unit, bool& result) noexcept {
    WORD type = 0u;
    if (GetStringTypeW(CT_CTYPE1, &unit, 1, &type) == FALSE) {
        return false;
    }
    result = (type & C1_SPACE) != 0u;
    return true;
}

}  // namespace

bool CopyBoundedGuiText(const char* text,
                        BoundedMenuText& output) noexcept {
    BoundedMenuText candidate{};
    if (!CopyGuiTextBytes(text, candidate.bytes.data(), &candidate.length)) {
        return false;
    }
    output = candidate;
    return true;
}

bool DecodeMenuTextLossless(const BoundedMenuText& text,
                            BoundedWideText& output) noexcept {
    if (text.length == 0u ||
        text.length > kMaximumMarkerTextUnits ||
        text.bytes[text.length] != '\0') {
        return false;
    }

    BoundedWideText candidate{};
    const int sourceLength = static_cast<int>(text.length);
    const int wideLength = MultiByteToWideChar(
        CP_ACP, MB_PRECOMPOSED, text.bytes.data(), sourceLength,
        candidate.units.data(), static_cast<int>(kMaximumMarkerTextUnits));
    if (wideLength <= 0 ||
        wideLength > static_cast<int>(kMaximumMarkerTextUnits)) {
        return false;
    }

    for (int index = 0; index < wideLength; ++index) {
        if (IsControl(candidate.units[static_cast<std::size_t>(index)])) {
            return false;
        }
    }

    std::array<char, kMaximumMarkerTextUnits + 1u> roundTrip{};
    BOOL usedDefault = FALSE;
    const int roundTripLength = WideCharToMultiByte(
        CP_ACP, WC_NO_BEST_FIT_CHARS, candidate.units.data(), wideLength,
        roundTrip.data(), static_cast<int>(kMaximumMarkerTextUnits), nullptr,
        &usedDefault);
    if (roundTripLength != sourceLength || usedDefault != FALSE) {
        return false;
    }
    for (int index = 0; index < sourceLength; ++index) {
        const auto offset = static_cast<std::size_t>(index);
        if (roundTrip[offset] != text.bytes[offset]) {
            return false;
        }
    }

    std::size_t first = 0u;
    const std::size_t end = static_cast<std::size_t>(wideLength);
    while (first < end) {
        bool space = false;
        if (!IsSpace(candidate.units[first], space)) {
            return false;
        }
        if (!space) {
            break;
        }
        ++first;
    }

    std::size_t last = end;
    while (last > first) {
        bool space = false;
        if (!IsSpace(candidate.units[last - 1u], space)) {
            return false;
        }
        if (!space) {
            break;
        }
        --last;
    }
    if (first == last) {
        return false;
    }

    const std::size_t trimmedLength = last - first;
    if (first != 0u) {
        for (std::size_t index = 0u; index < trimmedLength; ++index) {
            candidate.units[index] = candidate.units[first + index];
        }
    }
    candidate.length = static_cast<std::uint16_t>(trimmedLength);
    candidate.units[trimmedLength] = L'\0';
    output = candidate;
    return true;
}

}  // namespace campaign_completion
