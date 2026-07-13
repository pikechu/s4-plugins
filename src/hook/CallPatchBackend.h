#pragma once

#include <array>
#include <cstdint>

namespace campaign_completion {

enum class PatchState {
    Original,
    InstalledByUs,
    Conflict,
    Unavailable,
};

enum class PatchFailure {
    None,
    InstallFailed,
    Conflict,
    RestoreFailed,
    VerificationFailed,
    NotStarted,
};

class ICallPatchBackend {
public:
    virtual ~ICallPatchBackend() = default;

    virtual PatchFailure Install(
        std::uintptr_t callSite, std::uintptr_t destination,
        const std::array<std::uint8_t, 5>& expectedBytes) = 0;
    virtual PatchState Inspect() = 0;
    virtual PatchFailure Restore() = 0;
};

}  // namespace campaign_completion
