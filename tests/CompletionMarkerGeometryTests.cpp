#include "marker/CompletionMarkerGeometry.h"

#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::MarkerDrawCommand AeneasRow() {
    campaign_completion::MarkerDrawCommand row{};
    row.logicalSurfaceWidth = 800u;
    row.logicalSurfaceHeight = 600u;
    row.x = 115u;
    row.y = 142u;
    row.width = 271u;
    row.height = 30u;
    return row;
}

bool PointInside(const POINT& point, const RECT& clip) {
    return point.x >= clip.left && point.x < clip.right &&
           point.y >= clip.top && point.y < clip.bottom;
}

}  // namespace

int RunCompletionMarkerGeometryTests() {
    using namespace campaign_completion;

    const auto row = AeneasRow();
    const auto geometry =
        BuildMarkerCheckGeometry(row, 274, 1348u, 600u);
    Require(geometry.has_value(), "the live Aeneas geometry is accepted");
    Require(geometry->clip.left == 389 && geometry->clip.right == 660,
            "pillarbox shifts the logical row exactly once");
    Require(geometry->clip.top == 142 && geometry->clip.bottom == 172,
            "vertical coordinates remain logical surface coordinates");
    Require(geometry->points[2].x == 656,
            "check right edge is inset four pixels from the row");
    Require(geometry->points[2].x - geometry->points[0].x == 15,
            "height 30 produces a size-15 check");
    for (const auto& point : geometry->points) {
        Require(PointInside(point, geometry->clip),
                "all check points stay inside the row clip");
    }

    auto lowerClamp = row;
    lowerClamp.height = 24u;
    const auto lower =
        BuildMarkerCheckGeometry(lowerClamp, 274, 1348u, 600u);
    Require(lower.has_value() &&
                lower->points[2].x - lower->points[0].x == 12,
            "the lower check-size clamp is 12 pixels");

    auto upperClamp = row;
    upperClamp.height = 40u;
    const auto upper =
        BuildMarkerCheckGeometry(upperClamp, 274, 1348u, 600u);
    Require(upper.has_value() &&
                upper->points[2].x - upper->points[0].x == 16,
            "the upper check-size clamp is 16 pixels");

    Require(!BuildMarkerCheckGeometry(row, -1, 800u, 600u).has_value(),
            "negative pillarbox fails closed");

    auto zero = row;
    zero.height = 0u;
    Require(!BuildMarkerCheckGeometry(zero, 274, 1348u, 600u).has_value(),
            "zero-height rows fail closed");

    auto tooSmall = row;
    tooSmall.width = 15u;
    tooSmall.height = 11u;
    Require(!BuildMarkerCheckGeometry(tooSmall, 274, 1348u, 600u)
                 .has_value(),
            "rows unable to contain the check and inset fail closed");

    auto outside = row;
    outside.logicalSurfaceWidth = 300u;
    Require(!BuildMarkerCheckGeometry(outside, 0, 300u, 600u).has_value(),
            "logical out-of-bounds rows fail closed");

    Require(!BuildMarkerCheckGeometry(row, 274, 1348u, 599u).has_value(),
            "destination height must match the logical surface");
    Require(!BuildMarkerCheckGeometry(row, 274, 1347u, 600u).has_value(),
            "destination width must equal logical width plus both bars");

    auto overflowing = row;
    overflowing.logicalSurfaceWidth =
        (std::numeric_limits<DWORD>::max)();
    Require(!BuildMarkerCheckGeometry(
                 overflowing, (std::numeric_limits<INT32>::max)(),
                 (std::numeric_limits<DWORD>::max)(), 600u)
                 .has_value(),
            "destination-width arithmetic overflow fails closed");

    return 0;
}
