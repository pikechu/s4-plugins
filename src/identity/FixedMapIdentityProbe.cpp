#include "identity/FixedMapIdentityProbe.h"

#include <sstream>
#include <string_view>
#include <utility>

namespace campaign_completion {
namespace {

constexpr std::size_t kMaximumRawCaptures = 8u;

std::string_view CaptureFailureName(CaptureFailure failure) noexcept {
    switch (failure) {
        case CaptureFailure::None: return "none";
        case CaptureFailure::NoActiveEpoch: return "no-active-epoch";
        case CaptureFailure::NoCandidate: return "no-candidate";
        case CaptureFailure::UnknownList: return "unknown-list";
        case CaptureFailure::ListLeaseExpired: return "list-lease-expired";
        case CaptureFailure::Expired: return "expired";
        case CaptureFailure::Ambiguous: return "ambiguous";
        case CaptureFailure::Overflow: return "overflow";
    }
    return "unknown";
}

std::string_view PathFailureName(MapPathFailure failure) noexcept {
    switch (failure) {
        case MapPathFailure::None: return "none";
        case MapPathFailure::Empty: return "empty";
        case MapPathFailure::ControlCharacter: return "control-character";
        case MapPathFailure::RootedPath: return "rooted-path";
        case MapPathFailure::Traversal: return "traversal";
        case MapPathFailure::WrongRoot: return "wrong-root";
        case MapPathFailure::WrongExtension: return "wrong-extension";
        case MapPathFailure::MapRootUnavailable: return "map-root-unavailable";
        case MapPathFailure::CandidateUnavailable: return "candidate-unavailable";
        case MapPathFailure::NotRegularFile: return "not-regular-file";
        case MapPathFailure::FinalPathUnavailable: return "final-path-unavailable";
        case MapPathFailure::OutsideMapRoot: return "outside-map-root";
        case MapPathFailure::Utf8ConversionFailed: return "utf8-conversion";
    }
    return "unknown";
}

}  // namespace

FixedMapIdentityProbe::FixedMapIdentityProbe(std::filesystem::path gameRoot,
                                             RecordSink recordSink)
    : validator_(std::move(gameRoot)), recordSink_(std::move(recordSink)) {}

void FixedMapIdentityProbe::Observe(CapturedWidePath capture,
                                    std::uint64_t sequence,
                                    std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_ || !capture) {
        return;
    }
    if (pending_.size() >= kMaximumRawCaptures) {
        rawOverflow_ = true;
        return;
    }
    pending_.push_back({std::move(capture), sequence, nowMs});
}

void FixedMapIdentityProbe::ObserveListKind(FixedMapListKind listKind,
                                            std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_ || listKind == FixedMapListKind::Unknown ||
        listKind == currentKind_) {
        return;
    }
    currentKind_ = listKind;
    pending_.clear();
    rawOverflow_ = false;
    state_.BeginMenuEpoch(listKind, nowMs);
    Emit(std::string("fixed-map list_kind=") +
         std::string(FixedMapListKindName(listKind)));
}

void FixedMapIdentityProbe::ObserveBack() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) {
        return;
    }
    currentKind_ = FixedMapListKind::Unknown;
    pending_.clear();
    rawOverflow_ = false;
    state_.InvalidateMenuEpoch();
}

void FixedMapIdentityProbe::ObserveTopLevelPage(DWORD page) {
    if (page == 1u || page == 7u) {
        ObserveBack();
    }
}

void FixedMapIdentityProbe::ObserveMapInit(std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) {
        return;
    }

    MapPathFailure pathFailure = MapPathFailure::None;
    for (const auto& raw : pending_) {
        const auto validated = validator_.ValidateWide(raw.capture.value);
        if (validated) {
            state_.ObserveCapture(validated.relativePath, raw.sequence,
                                  raw.capturedAtMs);
        } else if (pathFailure == MapPathFailure::None) {
            pathFailure = validated.failure;
        }
    }
    pending_.clear();

    auto identity = state_.ObserveMapInit(nowMs);
    if (rawOverflow_) {
        identity.confidence = IdentityConfidence::Unknown;
        identity.failure = CaptureFailure::Overflow;
    }
    rawOverflow_ = false;

    std::ostringstream record;
    if (identity.confidence == IdentityConfidence::Confirmed) {
        record << "identity confirmed list_kind="
               << FixedMapListKindName(identity.listKind)
               << " path=" << identity.relativePath
               << " sequence=" << identity.sequence
               << " epoch=" << identity.menuEpoch;
    } else if (pathFailure != MapPathFailure::None &&
               identity.failure == CaptureFailure::NoCandidate) {
        record << "identity unknown reason=path-"
               << PathFailureName(pathFailure);
    } else {
        record << "identity unknown reason="
               << CaptureFailureName(identity.failure);
    }
    Emit(record.str());
    currentKind_ = FixedMapListKind::Unknown;
}

void FixedMapIdentityProbe::Disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) {
        return;
    }
    disabled_ = true;
    pending_.clear();
    rawOverflow_ = false;
    state_.InvalidateMenuEpoch();
    Emit("fixed-map probe disabled");
}

void FixedMapIdentityProbe::Emit(std::string record) {
    if (record == lastRecord_) {
        return;
    }
    lastRecord_ = record;
    if (recordSink_) {
        recordSink_(std::move(record));
    }
}

}  // namespace campaign_completion
