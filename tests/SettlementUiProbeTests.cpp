#include "victory/SettlementUiProbe.h"

#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunSettlementUiProbeTests() {
    using namespace campaign_completion;

    SettlementUiProbe invalid;
    Require(!invalid.Begin(0u, 100u), "zero map session is rejected");

    SettlementUiProbe probe;
    SettlementFeature first{};
    first.gfxCollection = 4u;
    first.mainTexture = 120u;
    first.valueLink = 55u;
    first.x = 10u;
    first.y = 20u;

    Require(probe.Begin(7u, 100u), "first capture begins");
    Require(!probe.Begin(7u, 101u), "capture cannot begin twice");
    probe.Observe(first);
    probe.Observe(first);
    Require(!probe.FinishIfDue(99u).has_value(),
            "clock regression does not finish a capture");
    Require(!probe.FinishIfDue(100u + kSettlementCaptureMs - 1u).has_value(),
            "capture remains open before deadline");
    const auto capture = probe.FinishIfDue(100u + kSettlementCaptureMs);
    Require(capture.has_value() && capture->sessionId == 7u &&
                capture->features.size() == 1u,
            "deadline returns one deduplicated feature");
    Require(!probe.FinishIfDue(100u + kSettlementCaptureMs + 1u).has_value(),
            "finished capture is one shot");

    SettlementUiProbe bounded;
    Require(bounded.Begin(8u, 0u), "bounded capture begins");
    for (std::size_t index = 0u; index < kMaxSettlementFeatures + 5u;
         ++index) {
        SettlementFeature feature{};
        feature.mainTexture = static_cast<std::uint16_t>(index);
        bounded.Observe(feature);
    }
    const auto capped = bounded.FinishIfDue(kSettlementCaptureMs);
    Require(capped.has_value() &&
                capped->features.size() == kMaxSettlementFeatures &&
                capped->truncated,
            "capture is count bounded and reports truncation");

    SettlementUiProbe disabled;
    Require(disabled.Begin(9u, 0u), "capture begins before Disable");
    disabled.Disable();
    Require(!disabled.Active() && !disabled.Begin(10u, 1u) &&
                !disabled.FinishIfDue(kSettlementCaptureMs).has_value(),
            "disabled probe clears state and remains inert");
    return 0;
}
