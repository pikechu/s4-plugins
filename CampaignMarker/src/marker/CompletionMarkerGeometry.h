#pragma once

#include "marker/FixedMapRowObserver.h"

#include <windows.h>

#include <array>
#include <optional>

namespace campaign_completion {

struct MarkerCheckGeometry final {
    RECT clip{};
    std::array<POINT, 3u> points{};
};

std::optional<MarkerCheckGeometry> BuildMarkerCheckGeometry(
    const MarkerDrawCommand& row, INT32 pillarboxWidth,
    DWORD destinationWidth, DWORD destinationHeight) noexcept;

}  // namespace campaign_completion
