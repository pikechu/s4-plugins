#include "identity/ListAttribution.h"

#include <array>
#include <vector>

namespace campaign_completion {
namespace {

bool HasExactFixedMapPages(const PageSnapshot& snapshot) {
    static const std::vector<DWORD> expected{4, 22, 23, 25};
    return snapshot.activePages == expected;
}

}  // namespace

std::string_view FixedMapListKindName(FixedMapListKind kind) noexcept {
    switch (kind) {
        case FixedMapListKind::Single:
            return "single";
        case FixedMapListKind::Multiplayer:
            return "multiplayer";
        case FixedMapListKind::Custom:
            return "custom";
        case FixedMapListKind::Unknown:
        default:
            return "unknown";
    }
}

ListAttribution::ListAttribution(TabControlMapping mapping) noexcept
    : mapping_(mapping) {}

void ListAttribution::ObservePages(const PageSnapshot& snapshot) {
    const bool present = HasExactFixedMapPages(snapshot);
    if (!present || !fixedMapPagesPresent_) {
        current_ = FixedMapListKind::Unknown;
    }
    fixedMapPagesPresent_ = present;
}

void ListAttribution::ObserveClick(UINT message, DWORD elementId) noexcept {
    if (!fixedMapPagesPresent_ || message != WM_LBUTTONUP) {
        return;
    }

    const int matchCount = static_cast<int>(elementId == mapping_.single) +
                           static_cast<int>(elementId == mapping_.multiplayer) +
                           static_cast<int>(elementId == mapping_.custom);
    if (matchCount != 1) {
        current_ = FixedMapListKind::Unknown;
    } else if (elementId == mapping_.single) {
        current_ = FixedMapListKind::Single;
    } else if (elementId == mapping_.multiplayer) {
        current_ = FixedMapListKind::Multiplayer;
    } else {
        current_ = FixedMapListKind::Custom;
    }
}

void ListAttribution::Reset() noexcept {
    fixedMapPagesPresent_ = false;
    current_ = FixedMapListKind::Unknown;
}

FixedMapListKind ListAttribution::Current() const noexcept {
    return current_;
}

}  // namespace campaign_completion
