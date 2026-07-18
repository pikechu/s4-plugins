#pragma once

#include <cstddef>
#include <string>

namespace campaign_completion {

enum class WideCaptureFailure {
    None,
    NullObject,
    HeaderUnreadable,
    LengthExceedsCapacity,
    LengthLimitExceeded,
    CapacityLimitExceeded,
    NullStorage,
    AddressOverflow,
    StorageUnreadable,
    MissingTerminator,
    AllocationFailure,
};

struct CapturedWidePath final {
    std::wstring value;
    WideCaptureFailure failure = WideCaptureFailure::None;

    explicit operator bool() const noexcept {
        return failure == WideCaptureFailure::None;
    }
};

CapturedWidePath CaptureMsvcX86WideString(const void* object,
                                         std::size_t maxChars) noexcept;

}  // namespace campaign_completion
