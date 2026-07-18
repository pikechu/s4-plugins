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

std::uint64_t ScaleFloor(std::uint64_t value, std::uint64_t destination,
                         std::uint64_t logical) noexcept {
    return value * destination / logical;
}

std::uint64_t ScaleCeil(std::uint64_t value, std::uint64_t destination,
                        std::uint64_t logical) noexcept {
    const auto product = value * destination;
    return product / logical + (product % logical != 0u ? 1u : 0u);
}

std::uint64_t ScaleNearest(std::uint64_t value,
                           std::uint64_t destination,
                           std::uint64_t logical) noexcept {
    const auto product = value * destination;
    const auto quotient = product / logical;
    const auto remainder = product % logical;
    return quotient +
           (remainder >= (logical / 2u + logical % 2u) ? 1u : 0u);
}

bool HasCompatibleAspect(std::uint64_t contentWidth,
                         std::uint64_t destinationHeight,
                         std::uint64_t logicalWidth,
                         std::uint64_t logicalHeight) noexcept {
    const auto horizontal = contentWidth * logicalHeight;
    const auto vertical = destinationHeight * logicalWidth;
    const auto difference = horizontal >= vertical
                                ? horizontal - vertical
                                : vertical - horizontal;
    const auto tolerance =
        (std::max)(logicalWidth, logicalHeight) / 2u;
    return difference <= tolerance;
}

}  // namespace

std::optional<MarkerCheckGeometry> BuildMarkerCheckGeometry(
    const MarkerDrawCommand& row, INT32 pillarboxWidth,
    DWORD destinationWidth, DWORD destinationHeight) noexcept {
    if (pillarboxWidth < 0 || destinationWidth == 0u ||
        destinationHeight == 0u || row.logicalSurfaceWidth == 0u ||
        row.logicalSurfaceHeight == 0u || row.width == 0u ||
        row.height == 0u) {
        return std::nullopt;
    }

    const auto logicalWidth =
        static_cast<std::uint64_t>(row.logicalSurfaceWidth);
    const auto logicalHeight =
        static_cast<std::uint64_t>(row.logicalSurfaceHeight);
    const auto pillar = static_cast<std::uint64_t>(pillarboxWidth);
    const auto destination = static_cast<std::uint64_t>(destinationWidth);
    const auto destinationY =
        static_cast<std::uint64_t>(destinationHeight);
    const auto totalPillar = pillar * 2u;
    if (totalPillar >= destination) {
        return std::nullopt;
    }
    const auto contentWidth = destination - totalPillar;
    if (!HasCompatibleAspect(contentWidth, destinationY, logicalWidth,
                             logicalHeight)) {
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
    if (static_cast<std::uint64_t>(row.width) < size + 10u ||
        static_cast<std::uint64_t>(row.height) <= size) {
        return std::nullopt;
    }

    const auto logicalCheckRight = logicalRight - 10u;
    const auto logicalCheckLeft = logicalCheckRight - size;
    const auto logicalCheckTop =
        logicalTop +
        (static_cast<std::uint64_t>(row.height) - size) / 2u;
    const auto logicalFirstY = logicalCheckTop + size / 2u;
    const auto logicalSecondX = logicalCheckLeft + size / 3u;
    const auto logicalSecondY = logicalCheckTop + size;

    const auto shiftedLeft =
        pillar + ScaleFloor(logicalLeft, contentWidth, logicalWidth);
    const auto shiftedRight =
        pillar + ScaleCeil(logicalRight, contentWidth, logicalWidth);
    const auto shiftedTop =
        ScaleFloor(logicalTop, destinationY, logicalHeight);
    const auto shiftedBottom =
        ScaleCeil(logicalBottom, destinationY, logicalHeight);
    const auto left =
        pillar + ScaleNearest(logicalCheckLeft, contentWidth, logicalWidth);
    const auto right =
        pillar + ScaleNearest(logicalCheckRight, contentWidth, logicalWidth);
    const auto top =
        ScaleNearest(logicalCheckTop, destinationY, logicalHeight);
    const auto firstY =
        ScaleNearest(logicalFirstY, destinationY, logicalHeight);
    const auto secondX =
        pillar + ScaleNearest(logicalSecondX, contentWidth, logicalWidth);
    const auto secondY =
        ScaleNearest(logicalSecondY, destinationY, logicalHeight);

    if (!FitsLong(shiftedLeft) || !FitsLong(shiftedRight) ||
        !FitsLong(shiftedTop) || !FitsLong(shiftedBottom) ||
        !FitsLong(left) || !FitsLong(right) || !FitsLong(top) ||
        !FitsLong(firstY) || !FitsLong(secondX) || !FitsLong(secondY) ||
        shiftedRight > destination || shiftedBottom > destinationY ||
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
