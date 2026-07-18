#include "victory/VictoryEventProbe.h"

#include <limits>
#include <tuple>

namespace campaign_completion {
namespace {

std::uint32_t IncrementBounded(std::uint32_t value) noexcept {
    if (value == (std::numeric_limits<std::uint32_t>::max)()) {
        return value;
    }
    return value + 1u;
}

NativeLocalResult ResultFor(std::uint32_t wParam) noexcept {
    if (wParam == 1u) {
        return NativeLocalResult::Won;
    }
    if (wParam == 0u) {
        return NativeLocalResult::Lost;
    }
    return NativeLocalResult::Malformed;
}

}  // namespace

bool NativeEventFields::operator==(
    const NativeEventFields& other) const noexcept {
    return std::tie(eventId, wParam, lParam, gameTick) ==
           std::tie(other.eventId, other.wParam, other.lParam,
                    other.gameTick);
}

void VictoryEventProbe::BeginSession(std::uint64_t sessionId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) {
        return;
    }
    snapshot_ = {};
    snapshot_.sessionId = sessionId;
    pending_ = false;
}

bool VictoryEventProbe::Observe(NativeEventFields fields) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || fields.eventId != kNativeTerminalEventId) {
        return false;
    }

    if (snapshot_.sessionId == 0u) {
        snapshot_.fields = fields;
        snapshot_.result = NativeLocalResult::None;
        snapshot_.orphans = IncrementBounded(snapshot_.orphans);
        pending_ = true;
        return true;
    }

    const auto result = ResultFor(fields.wParam);
    if (snapshot_.result == NativeLocalResult::None) {
        snapshot_.fields = fields;
        snapshot_.result = result;
    } else if (snapshot_.result == NativeLocalResult::Conflict) {
        snapshot_.duplicates = IncrementBounded(snapshot_.duplicates);
    } else if (snapshot_.result == result && snapshot_.fields == fields) {
        snapshot_.duplicates = IncrementBounded(snapshot_.duplicates);
    } else {
        snapshot_.fields = fields;
        snapshot_.result = NativeLocalResult::Conflict;
    }
    pending_ = true;
    return true;
}

std::optional<VictoryEventSnapshot>
VictoryEventProbe::ConsumePending() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || !pending_) {
        return std::nullopt;
    }
    pending_ = false;
    return snapshot_;
}

void VictoryEventProbe::Disable() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = {};
    pending_ = false;
    enabled_ = false;
}

}  // namespace campaign_completion
