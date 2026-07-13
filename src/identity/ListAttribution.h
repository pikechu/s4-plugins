#pragma once

#include "runtime/PageObservation.h"

#include <windows.h>

#include <string_view>

namespace campaign_completion {

enum class FixedMapListKind {
    Unknown,
    Single,
    Multiplayer,
    Custom,
};

std::string_view FixedMapListKindName(FixedMapListKind kind) noexcept;

struct TabControlMapping final {
    DWORD single;
    DWORD multiplayer;
    DWORD custom;
};

// Live calibration report rows C1-C3:
// docs/research/phase-2-2-tab-calibration.md
inline constexpr TabControlMapping kApprovedTabControls{2449, 2450, 2451};

class ListAttribution final {
public:
    explicit ListAttribution(TabControlMapping mapping) noexcept;

    void ObservePages(const PageSnapshot& snapshot);
    void ObserveClick(UINT message, DWORD elementId) noexcept;
    void Reset() noexcept;
    FixedMapListKind Current() const noexcept;

private:
    TabControlMapping mapping_;
    FixedMapListKind current_ = FixedMapListKind::Unknown;
    bool fixedMapPagesPresent_ = false;
};

}  // namespace campaign_completion
