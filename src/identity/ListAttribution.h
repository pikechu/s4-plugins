#pragma once

#include "runtime/PageObservation.h"

#include <windows.h>

namespace campaign_completion {

enum class FixedMapListKind {
    Unknown,
    Single,
    Multiplayer,
    Custom,
};

struct TabControlMapping final {
    DWORD single;
    DWORD multiplayer;
    DWORD custom;
};

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
