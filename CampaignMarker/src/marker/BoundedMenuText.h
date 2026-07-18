#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace campaign_completion {

inline constexpr std::size_t kMaximumMarkerTextUnits = 256u;

struct BoundedMenuText final {
    std::array<char, kMaximumMarkerTextUnits + 1u> bytes{};
    std::uint16_t length = 0u;
};

struct BoundedWideText final {
    std::array<wchar_t, kMaximumMarkerTextUnits + 1u> units{};
    std::uint16_t length = 0u;

    std::wstring_view view() const noexcept {
        return {units.data(), length};
    }
};

bool CopyBoundedGuiText(const char* text,
                        BoundedMenuText& output) noexcept;
bool DecodeMenuTextLossless(const BoundedMenuText& text,
                            BoundedWideText& output) noexcept;

}  // namespace campaign_completion
