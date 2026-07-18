#pragma once

#include "diagnostics/ModuleInventory.h"

#include <array>
#include <cstdint>

namespace campaign_completion {

struct FixedMapHookSiteSpec final {
    std::uint32_t callSiteRva;
    std::uint32_t originalTargetRva;
    std::array<std::uint8_t, 5> expectedCallBytes;
    std::uint32_t calleeStackCleanup;
};

inline constexpr FixedMapHookSiteSpec kApprovedFixedMapHookSite{
    0x000FEFA5u,
    0x000B1310u,
    {0xE8u, 0x66u, 0x23u, 0xFBu, 0xFFu},
    0x0000000Cu,
};

struct HookSectionSpan final {
    std::uint32_t virtualAddress = 0;
    std::uint32_t virtualSize = 0;
    bool executable = false;
};

enum class HookSiteFailure {
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
    BytesMismatch,
    TargetMismatch,
};

struct HookSiteAdmission final {
    std::uintptr_t callSite = 0;
    std::uintptr_t originalTarget = 0;
    HookSiteFailure failure = HookSiteFailure::None;

    explicit operator bool() const noexcept {
        return failure == HookSiteFailure::None;
    }
};

class HookSiteLayout final {
public:
    static HookSiteAdmission Create(const ModuleInfo& executable) noexcept;

    static HookSiteAdmission FromEvidenceForTesting(
        const ModuleInfo& executable,
        const HookSectionSpan& textSection,
        const FixedMapHookSiteSpec& spec,
        const std::array<std::uint8_t, 5>& liveBytes) noexcept;
};

}  // namespace campaign_completion
