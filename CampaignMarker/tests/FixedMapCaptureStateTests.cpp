#include "identity/FixedMapCaptureState.h"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunFixedMapCaptureStateTests() {
    using campaign_completion::CaptureFailure;
    using campaign_completion::FixedMapCaptureState;
    using campaign_completion::FixedMapListKind;
    using campaign_completion::IdentityConfidence;
    using campaign_completion::kCaptureLeaseMs;
    using campaign_completion::kListLeaseMs;

    static_assert(kCaptureLeaseMs == 15000u);
    static_assert(kListLeaseMs == 600000u);

    FixedMapCaptureState belowBoundary;
    belowBoundary.BeginMenuEpoch(FixedMapListKind::Single, 1000u);
    belowBoundary.ObserveCapture("Map\\User\\A.map", 1u, 2000u);
    const auto confirmed = belowBoundary.ObserveMapInit(16999u);
    Require(confirmed.confidence == IdentityConfidence::Confirmed,
            "capture valid below expiry boundary");
    Require(confirmed.failure == CaptureFailure::None,
            "confirmed capture has no failure");
    Require(confirmed.listKind == FixedMapListKind::Single,
            "single list kind preserved");
    Require(confirmed.relativePath == "Map\\User\\A.map",
            "relative path preserved");
    Require(confirmed.sequence == 1u, "capture sequence preserved");
    Require(confirmed.menuEpoch == 1u, "first epoch is stable");

    FixedMapCaptureState exactBoundary;
    exactBoundary.BeginMenuEpoch(FixedMapListKind::Single, 1000u);
    exactBoundary.ObserveCapture("Map\\User\\A.map", 2u, 2000u);
    Require(exactBoundary.ObserveMapInit(17000u).failure ==
                CaptureFailure::Expired,
            "capture expired at exact boundary");

    FixedMapCaptureState repeated;
    repeated.BeginMenuEpoch(FixedMapListKind::Single, 0u);
    repeated.ObserveCapture("Map\\User\\Repeat.map", 9u, 1u);
    repeated.ObserveCapture("map\\user\\repeat.MAP", 5u, 14999u);
    const auto collapsed = repeated.ObserveMapInit(29998u);
    Require(collapsed.confidence == IdentityConfidence::Confirmed,
            "identical repeat refreshes capture time");
    Require(collapsed.sequence == 5u,
            "identical repeat preserves lowest sequence");
    Require(collapsed.relativePath == "Map\\User\\Repeat.map",
            "identical repeat preserves first spelling");

    FixedMapCaptureState ambiguous;
    ambiguous.BeginMenuEpoch(FixedMapListKind::Custom, 100u);
    ambiguous.ObserveCapture("Map\\User\\A.map", 1u, 200u);
    ambiguous.ObserveCapture("Map\\User\\B.map", 2u, 300u);
    Require(ambiguous.ObserveMapInit(400u).failure ==
                CaptureFailure::Ambiguous,
            "different paths make epoch ambiguous");

    FixedMapCaptureState unknownList;
    unknownList.BeginMenuEpoch(FixedMapListKind::Unknown, 100u);
    unknownList.ObserveCapture("Map\\User\\A.map", 1u, 200u);
    Require(unknownList.ObserveMapInit(300u).failure ==
                CaptureFailure::UnknownList,
            "unknown list cannot confirm identity");

    FixedMapCaptureState expiredList;
    expiredList.BeginMenuEpoch(FixedMapListKind::Multiplayer, 1000u);
    expiredList.ObserveCapture("Map\\User\\A.map", 1u, 600999u);
    Require(expiredList.ObserveMapInit(1000u + kListLeaseMs).failure ==
                CaptureFailure::ListLeaseExpired,
            "list lease expires at exact boundary");

    FixedMapCaptureState changedTab;
    changedTab.BeginMenuEpoch(FixedMapListKind::Single, 100u);
    changedTab.ObserveCapture("Map\\User\\Old.map", 1u, 200u);
    changedTab.BeginMenuEpoch(FixedMapListKind::Custom, 300u);
    changedTab.ObserveCapture("Map\\User\\New.map", 2u, 400u);
    const auto changed = changedTab.ObserveMapInit(500u);
    Require(changed.confidence == IdentityConfidence::Confirmed,
            "new tab epoch can confirm");
    Require(changed.listKind == FixedMapListKind::Custom,
            "new tab replaces old list kind");
    Require(changed.relativePath == "Map\\User\\New.map",
            "new tab cannot reuse old capture");
    Require(changed.menuEpoch == 2u, "new tab increments epoch");

    FixedMapCaptureState invalidated;
    invalidated.BeginMenuEpoch(FixedMapListKind::Single, 100u);
    invalidated.ObserveCapture("Map\\User\\A.map", 1u, 200u);
    invalidated.InvalidateMenuEpoch();
    Require(invalidated.ObserveMapInit(300u).failure ==
                CaptureFailure::NoActiveEpoch,
            "Back or top-level invalidation clears epoch");

    FixedMapCaptureState consumed;
    consumed.BeginMenuEpoch(FixedMapListKind::Single, 100u);
    consumed.ObserveCapture("Map\\User\\A.map", 1u, 200u);
    Require(consumed.ObserveMapInit(300u).confidence ==
                IdentityConfidence::Confirmed,
            "first map-init consumes candidate");
    Require(consumed.ObserveMapInit(301u).failure ==
                CaptureFailure::NoCandidate,
            "candidate is consumed exactly once");

    FixedMapCaptureState previousEpoch;
    previousEpoch.BeginMenuEpoch(FixedMapListKind::Single, 100u);
    previousEpoch.ObserveCapture("Map\\User\\Old.map", 1u, 200u);
    previousEpoch.BeginMenuEpoch(FixedMapListKind::Multiplayer, 300u);
    Require(previousEpoch.ObserveMapInit(400u).failure ==
                CaptureFailure::NoCandidate,
            "previous epoch candidate cannot be reused");

    for (const auto kind : {FixedMapListKind::Single,
                            FixedMapListKind::Multiplayer,
                            FixedMapListKind::Custom}) {
        FixedMapCaptureState distinct;
        distinct.BeginMenuEpoch(kind, 100u);
        distinct.ObserveCapture("Map\\User\\Kind.map", 1u, 200u);
        Require(distinct.ObserveMapInit(300u).listKind == kind,
                "fixed-map list kinds remain distinct");
    }

    FixedMapCaptureState overflow;
    overflow.BeginMenuEpoch(FixedMapListKind::Single, 100u);
    for (std::uint64_t index = 0; index < 9u; ++index) {
        overflow.ObserveCapture("Map\\User\\" + std::to_string(index) +
                                    ".map",
                                index + 1u, 200u + index);
    }
    Require(overflow.ObserveMapInit(300u).failure ==
                CaptureFailure::Overflow,
            "more than eight distinct captures fail closed");

    return 0;
}
