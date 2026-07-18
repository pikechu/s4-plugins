#pragma once

#include "diagnostics/ModuleInventory.h"

#include <array>
#include <cstdint>

namespace campaign_completion {

struct NativeEventLayoutSpec final {
    std::uint32_t engineSlotRva;
    std::uint32_t registerHandleRva;
    std::uint32_t unregisterHandleRva;
    std::array<std::uint8_t, 5> expectedRegisterPrefix;
    std::array<std::uint8_t, 5> expectedUnregisterPrefix;
};

inline constexpr NativeEventLayoutSpec kApprovedNativeEventLayout{
    0x0106B11Cu,
    0x0005A8A0u,
    0x0005A990u,
    {0x55u, 0x8Bu, 0xECu, 0x6Au, 0xFFu},
    {0x55u, 0x8Bu, 0xECu, 0x51u, 0xA1u},
};

struct NativeEventSectionSpan final {
    std::uint32_t virtualAddress = 0u;
    std::uint32_t virtualSize = 0u;
    bool executable = false;
};

struct NativeEventAdmissionEvidence final {
    NativeEventSectionSpan textSection{};
    std::array<std::uint8_t, 50> registerBytes{};
    std::array<std::uint8_t, 13> unregisterBytes{};
    bool registerMemoryReadable = false;
    bool unregisterMemoryReadable = false;
    bool slotMemoryReadable = false;
    bool engineMemoryReadable = false;
};

enum class NativeEventAdmissionFailure {
    None,
    VersionMismatch,
    HashMismatch,
    FileUnavailable,
    InvalidPeImage,
    TextSectionMissing,
    OutsideModule,
    WrongSection,
    AddressOverflow,
    MemoryUnavailable,
    RegisterBytesMismatch,
    UnregisterBytesMismatch,
    EngineSlotOperandMismatch,
    EngineUnavailable,
};

struct NativeEventAdmission final {
    std::uintptr_t engineSlot = 0u;
    std::uintptr_t registerHandle = 0u;
    std::uintptr_t unregisterHandle = 0u;
    NativeEventAdmissionFailure failure = NativeEventAdmissionFailure::None;

    explicit operator bool() const noexcept {
        return failure == NativeEventAdmissionFailure::None;
    }

    static NativeEventAdmission Create(const ModuleInfo& executable) noexcept;

    static NativeEventAdmission FromEvidenceForTesting(
        const ModuleInfo& executable, const NativeEventLayoutSpec& spec,
        const NativeEventAdmissionEvidence& evidence) noexcept;
};

}  // namespace campaign_completion
