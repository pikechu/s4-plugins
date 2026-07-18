#include "identity/FixedMapCaptureState.h"

#include <windows.h>

#include <algorithm>
#include <climits>
#include <string_view>
#include <utility>

namespace campaign_completion {
namespace {

constexpr std::size_t kMaximumPendingCaptures = 8u;

bool HasExpired(std::uint64_t startedAtMs, std::uint64_t nowMs,
                std::uint64_t leaseMs) noexcept {
    return nowMs < startedAtMs || nowMs - startedAtMs >= leaseMs;
}

bool ToWide(std::string_view utf8, std::wstring& result) {
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    const int inputLength = static_cast<int>(utf8.size());
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             utf8.data(), inputLength, nullptr,
                                             0);
    if (required <= 0) {
        return false;
    }
    result.resize(static_cast<std::size_t>(required));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                               inputLength, result.data(), required) == required;
}

bool EqualPathIgnoreCase(std::string_view left, std::string_view right) {
    std::wstring wideLeft;
    std::wstring wideRight;
    if (!ToWide(left, wideLeft) || !ToWide(right, wideRight) ||
        wideLeft.size() > static_cast<std::size_t>(INT_MAX) ||
        wideRight.size() > static_cast<std::size_t>(INT_MAX)) {
        return left == right;
    }
    return CompareStringOrdinal(
               wideLeft.data(), static_cast<int>(wideLeft.size()),
               wideRight.data(), static_cast<int>(wideRight.size()), TRUE) ==
           CSTR_EQUAL;
}

LoadedMapIdentity Failed(FixedMapListKind listKind, std::uint64_t epoch,
                         CaptureFailure failure) {
    LoadedMapIdentity result{};
    result.listKind = listKind;
    result.menuEpoch = epoch;
    result.failure = failure;
    return result;
}

}  // namespace

void FixedMapCaptureState::BeginMenuEpoch(FixedMapListKind listKind,
                                          std::uint64_t nowMs) {
    ++epochCounter_;
    listKind_ = listKind;
    epochStartedAtMs_ = nowMs;
    active_ = true;
    consumed_ = false;
    ClearPending();
}

void FixedMapCaptureState::InvalidateMenuEpoch() noexcept {
    active_ = false;
    consumed_ = false;
    listKind_ = FixedMapListKind::Unknown;
    ClearPending();
}

void FixedMapCaptureState::ObserveCapture(std::string relativePath,
                                          std::uint64_t sequence,
                                          std::uint64_t nowMs) {
    if (!active_ || consumed_ ||
        HasExpired(epochStartedAtMs_, nowMs, kListLeaseMs)) {
        return;
    }

    const auto existing = std::find_if(
        pending_.begin(), pending_.end(), [&](const PendingCapture& pending) {
            return EqualPathIgnoreCase(pending.relativePath, relativePath);
        });
    if (existing != pending_.end()) {
        existing->sequence = (std::min)(existing->sequence, sequence);
        existing->capturedAtMs = nowMs;
        return;
    }

    if (pending_.size() >= kMaximumPendingCaptures) {
        overflow_ = true;
        return;
    }
    if (!pending_.empty()) {
        ambiguous_ = true;
    }
    pending_.push_back({std::move(relativePath), sequence, nowMs});
}

LoadedMapIdentity FixedMapCaptureState::ObserveMapInit(
    std::uint64_t nowMs) {
    LoadedMapIdentity result{};
    if (!active_) {
        result = Failed(FixedMapListKind::Unknown, epochCounter_,
                        CaptureFailure::NoActiveEpoch);
    } else if (consumed_) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::NoCandidate);
    } else if (HasExpired(epochStartedAtMs_, nowMs, kListLeaseMs)) {
        result = Failed(listKind_, epochCounter_,
                        CaptureFailure::ListLeaseExpired);
    } else if (overflow_) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::Overflow);
    } else if (ambiguous_) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::Ambiguous);
    } else if (pending_.empty()) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::NoCandidate);
    } else if (listKind_ == FixedMapListKind::Unknown) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::UnknownList);
    } else if (HasExpired(pending_.front().capturedAtMs, nowMs,
                          kCaptureLeaseMs)) {
        result = Failed(listKind_, epochCounter_, CaptureFailure::Expired);
    } else {
        result.listKind = listKind_;
        result.relativePath = pending_.front().relativePath;
        result.sequence = pending_.front().sequence;
        result.menuEpoch = epochCounter_;
        result.confidence = IdentityConfidence::Confirmed;
        result.failure = CaptureFailure::None;
    }

    consumed_ = active_;
    ClearPending();
    return result;
}

void FixedMapCaptureState::ClearPending() noexcept {
    pending_.clear();
    ambiguous_ = false;
    overflow_ = false;
}

}  // namespace campaign_completion
