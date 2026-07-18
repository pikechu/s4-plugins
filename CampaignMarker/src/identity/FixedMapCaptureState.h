#pragma once

#include "identity/ListAttribution.h"

#include <cstdint>
#include <string>
#include <vector>

namespace campaign_completion {

inline constexpr std::uint64_t kListLeaseMs = 600000u;
inline constexpr std::uint64_t kCaptureLeaseMs = 15000u;

enum class IdentityConfidence {
    Unknown,
    Confirmed,
};

enum class CaptureFailure {
    None,
    NoActiveEpoch,
    NoCandidate,
    UnknownList,
    ListLeaseExpired,
    Expired,
    Ambiguous,
    Overflow,
};

struct LoadedMapIdentity final {
    FixedMapListKind listKind = FixedMapListKind::Unknown;
    std::string relativePath;
    std::uint64_t sequence = 0;
    std::uint64_t menuEpoch = 0;
    IdentityConfidence confidence = IdentityConfidence::Unknown;
    CaptureFailure failure = CaptureFailure::NoCandidate;
};

class FixedMapCaptureState final {
public:
    void BeginMenuEpoch(FixedMapListKind listKind,
                        std::uint64_t nowMs);
    void InvalidateMenuEpoch() noexcept;
    void ObserveCapture(std::string relativePath, std::uint64_t sequence,
                        std::uint64_t nowMs);
    LoadedMapIdentity ObserveMapInit(std::uint64_t nowMs);

private:
    struct PendingCapture final {
        std::string relativePath;
        std::uint64_t sequence = 0;
        std::uint64_t capturedAtMs = 0;
    };

    void ClearPending() noexcept;

    std::vector<PendingCapture> pending_;
    FixedMapListKind listKind_ = FixedMapListKind::Unknown;
    std::uint64_t epochCounter_ = 0;
    std::uint64_t epochStartedAtMs_ = 0;
    bool active_ = false;
    bool consumed_ = false;
    bool ambiguous_ = false;
    bool overflow_ = false;
};

}  // namespace campaign_completion
