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

std::string_view WideFailureName(WideCaptureFailure failure) noexcept {
    switch (failure) {
        case WideCaptureFailure::None: return "none";
        case WideCaptureFailure::NullObject: return "null-object";
        case WideCaptureFailure::HeaderUnreadable: return "header-unreadable";
        case WideCaptureFailure::LengthExceedsCapacity:
            return "length-exceeds-capacity";
        case WideCaptureFailure::LengthLimitExceeded:
            return "length-limit-exceeded";
        case WideCaptureFailure::CapacityLimitExceeded:
            return "capacity-limit-exceeded";
        case WideCaptureFailure::NullStorage: return "null-storage";
        case WideCaptureFailure::AddressOverflow: return "address-overflow";
        case WideCaptureFailure::StorageUnreadable:
            return "storage-unreadable";
        case WideCaptureFailure::MissingTerminator:
            return "missing-terminator";
        case WideCaptureFailure::AllocationFailure:
            return "allocation-failure";
    }
    return "unknown";
}

}  // namespace

FixedMapIdentityProbe::FixedMapIdentityProbe(std::filesystem::path gameRoot,
                                             RecordSink recordSink,
                                             TraceSink traceSink)
    : validator_(std::move(gameRoot)),
      recordSink_(std::move(recordSink)),
      traceSink_(std::move(traceSink)) {}

void FixedMapIdentityProbe::Observe(CapturedWidePath capture,
                                    std::uint64_t sequence,
                                    std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) {
        return;
    }
    if (adapterEntered_ >= kMaximumRawCaptures) {
        rawOverflow_ = true;
        return;
    }
    ++adapterEntered_;
    if (!capture) {
        if (firstWideFailure_ == WideCaptureFailure::None) {
            firstWideFailure_ = capture.failure;
        }
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
    adapterEntered_ = 0;
    firstWideFailure_ = WideCaptureFailure::None;
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
    adapterEntered_ = 0;
    firstWideFailure_ = WideCaptureFailure::None;
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

    const auto adapterEntered = adapterEntered_;
    const auto firstWideFailure = firstWideFailure_;
    const bool wideCaptureSucceeded = !pending_.empty();
    MapPathFailure pathFailure = MapPathFailure::None;
    bool pathValidated = false;
    for (const auto& raw : pending_) {
        const auto validated = validator_.ValidateWide(raw.capture.value);
        if (validated) {
            pathValidated = true;
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

    EmitTrace("adapter-entered=" + std::to_string(adapterEntered));
    if (wideCaptureSucceeded) {
        EmitTrace("wide-capture=success");
        if (pathValidated) {
            EmitTrace("path-validation=success");
        } else if (pathFailure != MapPathFailure::None) {
            EmitTrace("path-validation=" +
                      std::string(PathFailureName(pathFailure)));
        }
    } else if (firstWideFailure != WideCaptureFailure::None) {
        EmitTrace("wide-capture=" +
                  std::string(WideFailureName(firstWideFailure)));
    }

    std::string association;
    if (identity.failure == CaptureFailure::Overflow) {
        association = "overflow";
    } else if (identity.confidence == IdentityConfidence::Confirmed) {
        association = "confirmed";
    } else if (wideCaptureSucceeded && !pathValidated &&
               pathFailure != MapPathFailure::None) {
        association = "path-" + std::string(PathFailureName(pathFailure));
    } else if (!wideCaptureSucceeded &&
               firstWideFailure != WideCaptureFailure::None) {
        association =
            "wide-" + std::string(WideFailureName(firstWideFailure));
    } else {
        association = std::string(CaptureFailureName(identity.failure));
    }
    EmitTrace("map-init-association=" + association);

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
    } else if (firstWideFailure != WideCaptureFailure::None &&
               identity.failure == CaptureFailure::NoCandidate) {
        record << "identity unknown reason=wide-"
               << WideFailureName(firstWideFailure);
    } else {
        record << "identity unknown reason="
               << CaptureFailureName(identity.failure);
    }
    Emit(record.str());
    currentKind_ = FixedMapListKind::Unknown;
    adapterEntered_ = 0;
    firstWideFailure_ = WideCaptureFailure::None;
}

void FixedMapIdentityProbe::Disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) {
        return;
    }
    disabled_ = true;
    pending_.clear();
    adapterEntered_ = 0;
    firstWideFailure_ = WideCaptureFailure::None;
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

void FixedMapIdentityProbe::EmitTrace(std::string record) {
    if (traceSink_) {
        traceSink_(std::move(record));
    }
}

}  // namespace campaign_completion
