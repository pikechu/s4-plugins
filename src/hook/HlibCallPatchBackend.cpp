#include "hook/HlibCallPatchBackend.h"

#include "S4ModApi.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace campaign_completion {
namespace {

constexpr std::array<std::uint8_t, 5> kOriginalBytes{
    0xE8u, 0x66u, 0x23u, 0xFBu, 0xFFu};
static constexpr hlib::Patch::BYTE5 kExpected{
    {0xE8u, 0x66u, 0x23u, 0xFBu, 0xFFu}};

bool ReadBytes(std::uintptr_t address,
               std::array<std::uint8_t, 5>& result) noexcept {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory,
                     sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u) {
        return false;
    }
    const auto regionBase = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    if (address < regionBase) {
        return false;
    }
    const auto offset = address - regionBase;
    if (offset > memory.RegionSize || result.size() > memory.RegionSize - offset) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        std::memcpy(result.data(), reinterpret_cast<const void*>(address),
                    result.size());
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(result.data(), reinterpret_cast<const void*>(address),
                result.size());
    return true;
#endif
}

bool BuildInstalledBytes(std::uintptr_t callSite, std::uintptr_t destination,
                         std::array<std::uint8_t, 5>& result) noexcept {
    const auto next = static_cast<std::int64_t>(callSite) + 5;
    const auto relative = static_cast<std::int64_t>(destination) - next;
    if (relative < (std::numeric_limits<std::int32_t>::min)() ||
        relative > (std::numeric_limits<std::int32_t>::max)()) {
        return false;
    }
    const auto bits = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(relative));
    result = {0xE8u, static_cast<std::uint8_t>(bits & 0xFFu),
              static_cast<std::uint8_t>((bits >> 8u) & 0xFFu),
              static_cast<std::uint8_t>((bits >> 16u) & 0xFFu),
              static_cast<std::uint8_t>((bits >> 24u) & 0xFFu)};
    return true;
}

}  // namespace

struct HlibCallPatchBackend::Impl final {
    std::unique_ptr<hlib::CallPatch> patch;
    std::uintptr_t callSite = 0;
    std::uintptr_t destination = 0;
    std::array<std::uint8_t, 5> installedBytes{};
};

HlibCallPatchBackend::HlibCallPatchBackend() : impl_(std::make_unique<Impl>()) {}

HlibCallPatchBackend::~HlibCallPatchBackend() = default;

PatchFailure HlibCallPatchBackend::Install(
    std::uintptr_t callSite, std::uintptr_t destination,
    const std::array<std::uint8_t, 5>& expectedBytes) {
    if (impl_->patch != nullptr || expectedBytes != kOriginalBytes ||
        callSite > (std::numeric_limits<DWORD>::max)() ||
        destination > (std::numeric_limits<DWORD>::max)() ||
        !BuildInstalledBytes(callSite, destination, impl_->installedBytes)) {
        return PatchFailure::InstallFailed;
    }

    std::array<std::uint8_t, 5> live{};
    if (!ReadBytes(callSite, live)) {
        return PatchFailure::InstallFailed;
    }
    if (live != kOriginalBytes) {
        return PatchFailure::Conflict;
    }

    impl_->callSite = callSite;
    impl_->destination = destination;
    impl_->patch = std::make_unique<hlib::CallPatch>(
        static_cast<UINT64>(callSite), static_cast<DWORD>(destination),
        &kExpected, 0);
    if (!impl_->patch->patch()) {
        return PatchFailure::InstallFailed;
    }
    return Inspect() == PatchState::InstalledByUs
               ? PatchFailure::None
               : PatchFailure::VerificationFailed;
}

PatchState HlibCallPatchBackend::Inspect() {
    if (impl_->callSite == 0u) {
        return PatchState::Unavailable;
    }
    std::array<std::uint8_t, 5> live{};
    if (!ReadBytes(impl_->callSite, live)) {
        return PatchState::Unavailable;
    }
    if (live == kOriginalBytes) {
        return PatchState::Original;
    }
    if (live == impl_->installedBytes) {
        return PatchState::InstalledByUs;
    }
    return PatchState::Conflict;
}

PatchFailure HlibCallPatchBackend::Restore() {
    const auto before = Inspect();
    if (before != PatchState::InstalledByUs) {
        return before == PatchState::Conflict ? PatchFailure::Conflict
                                              : PatchFailure::RestoreFailed;
    }
    if (impl_->patch == nullptr || !impl_->patch->unpatch()) {
        return PatchFailure::RestoreFailed;
    }
    return Inspect() == PatchState::Original
               ? PatchFailure::None
               : PatchFailure::VerificationFailed;
}

DirectOriginalLoadInvoker::DirectOriginalLoadInvoker(
    std::uintptr_t originalTarget) noexcept
    : originalTarget_(originalTarget) {}

void DirectOriginalLoadInvoker::Invoke(void* mapFile, const void* path,
                                       DWORD mode, DWORD validate) {
    using OriginalLoad = void(__thiscall*)(void*, const void*, DWORD, DWORD);
    reinterpret_cast<OriginalLoad>(originalTarget_)(mapFile, path, mode,
                                                    validate);
}

}  // namespace campaign_completion
