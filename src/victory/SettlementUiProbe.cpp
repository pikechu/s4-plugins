#include "victory/SettlementUiProbe.h"

#include <algorithm>
#include <tuple>
#include <utility>

namespace campaign_completion {

bool SettlementFeature::operator==(
    const SettlementFeature& other) const noexcept {
    return std::tie(gfxCollection, containerType, x, y, width, height,
                    mainTexture, valueLink, buttonPressedTexture, tooltipLink,
                    imageStyle, effects, textStyle, showTexture, backTexture) ==
           std::tie(other.gfxCollection, other.containerType, other.x, other.y,
                    other.width, other.height, other.mainTexture,
                    other.valueLink, other.buttonPressedTexture,
                    other.tooltipLink, other.imageStyle, other.effects,
                    other.textStyle, other.showTexture, other.backTexture);
}

bool SettlementUiProbe::Begin(std::uint64_t sessionId,
                              std::uint64_t nowMs) noexcept {
    if (!enabled_ || active_ || sessionId == 0u) {
        return false;
    }

    capture_.sessionId = sessionId;
    capture_.features.clear();
    capture_.truncated = false;
    startedAtMs_ = nowMs;
    active_ = true;
    return true;
}

void SettlementUiProbe::Observe(const SettlementFeature& feature) noexcept {
    if (!enabled_ || !active_) {
        return;
    }

    try {
        if (std::find(capture_.features.begin(), capture_.features.end(),
                      feature) != capture_.features.end()) {
            return;
        }
        if (capture_.features.size() >= kMaxSettlementFeatures) {
            capture_.truncated = true;
            return;
        }
        capture_.features.push_back(feature);
    } catch (...) {
        capture_.truncated = true;
    }
}

std::optional<SettlementCapture> SettlementUiProbe::FinishIfDue(
    std::uint64_t nowMs) noexcept {
    if (!enabled_ || !active_ || nowMs < startedAtMs_ ||
        nowMs - startedAtMs_ < kSettlementCaptureMs) {
        return std::nullopt;
    }

    active_ = false;
    startedAtMs_ = 0u;
    std::optional<SettlementCapture> result{std::move(capture_)};
    capture_ = {};
    return result;
}

void SettlementUiProbe::Disable() noexcept {
    capture_ = {};
    startedAtMs_ = 0u;
    active_ = false;
    enabled_ = false;
}

}  // namespace campaign_completion
