#include "marker/CompletionMarkerGeometry.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace campaign_completion {
namespace {

bool FitsLong(std::uint64_t value) noexcept {
    return value <=
           static_cast<std::uint64_t>((std::numeric_limits<LONG>::max)());
}

}  // namespace

std::optional<MarkerCheckGeometry> BuildMarkerCheckGeometry(
    const MarkerDrawCommand& row, INT32 pillarboxWidth,
    DWORD destinationWidth, DWORD destinationHeight) noexcept {
    if (pillarboxWidth < 0 || row.logicalSurfaceWidth == 0u ||
        row.logicalSurfaceHeight == 0u || row.width == 0u ||
        row.height == 0u ||
        destinationHeight != row.logicalSurfaceHeight) {
        return std::nullopt;
    }

    const auto logicalWidth =
        static_cast<std::uint64_t>(row.logicalSurfaceWidth);
    const auto logicalHeight =
        static_cast<std::uint64_t>(row.logicalSurfaceHeight);
    const auto pillar = static_cast<std::uint64_t>(pillarboxWidth);
    const auto expectedDestinationWidth = logicalWidth + pillar * 2u;
    if (expectedDestinationWidth >
            static_cast<std::uint64_t>(
                (std::numeric_limits<DWORD>::max)()) ||
        expectedDestinationWidth !=
            static_cast<std::uint64_t>(destinationWidth)) {
        return std::nullopt;
    }

    const auto logicalLeft = static_cast<std::uint64_t>(row.x);
    const auto logicalTop = static_cast<std::uint64_t>(row.y);
    const auto logicalRight =
        logicalLeft + static_cast<std::uint64_t>(row.width);
    const auto logicalBottom =
        logicalTop + static_cast<std::uint64_t>(row.height);
    if (logicalRight > logicalWidth || logicalBottom > logicalHeight) {
        return std::nullopt;
    }

    const auto size = std::clamp<std::uint64_t>(
        static_cast<std::uint64_t>(row.height) / 2u, 12u, 16u);
    if (static_cast<std::uint64_t>(row.width) < size + 4u ||
        static_cast<std::uint64_t>(row.height) <= size) {
        return std::nullopt;
    }

    const auto shiftedLeft = pillar + logicalLeft;
    const auto shiftedRight = pillar + logicalRight;
    const auto shiftedTop = logicalTop;
    const auto shiftedBottom = logicalBottom;
    const auto right = shiftedRight - 4u;
    const auto left = right - size;
    const auto top = shiftedTop +
                     (static_cast<std::uint64_t>(row.height) - size) / 2u;
    const auto firstY = top + size / 2u;
    const auto secondX = left + size / 3u;
    const auto secondY = top + size;

    if (!FitsLong(shiftedLeft) || !FitsLong(shiftedRight) ||
        !FitsLong(shiftedTop) || !FitsLong(shiftedBottom) ||
        !FitsLong(left) || !FitsLong(right) || !FitsLong(top) ||
        !FitsLong(firstY) || !FitsLong(secondX) || !FitsLong(secondY) ||
        left < shiftedLeft || right >= shiftedRight ||
        firstY >= shiftedBottom || secondY >= shiftedBottom) {
        return std::nullopt;
    }

    MarkerCheckGeometry geometry{};
    geometry.clip = {
        static_cast<LONG>(shiftedLeft), static_cast<LONG>(shiftedTop),
        static_cast<LONG>(shiftedRight), static_cast<LONG>(shiftedBottom)};
    geometry.points = {{
        {static_cast<LONG>(left), static_cast<LONG>(firstY)},
        {static_cast<LONG>(secondX), static_cast<LONG>(secondY)},
        {static_cast<LONG>(right), static_cast<LONG>(top)},
    }};
    return geometry;
}

}  // namespace campaign_completion
