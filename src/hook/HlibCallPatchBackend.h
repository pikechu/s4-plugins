#pragma once

#include "hook/CallPatchBackend.h"
#include "hook/FixedMapLoadHook.h"

#include <memory>

namespace campaign_completion {

class HlibCallPatchBackend final : public ICallPatchBackend {
public:
    HlibCallPatchBackend();
    ~HlibCallPatchBackend() override;

    HlibCallPatchBackend(const HlibCallPatchBackend&) = delete;
    HlibCallPatchBackend& operator=(const HlibCallPatchBackend&) = delete;

    PatchFailure Install(
        std::uintptr_t callSite, std::uintptr_t destination,
        const std::array<std::uint8_t, 5>& expectedBytes) override;
    PatchState Inspect() override;
    PatchFailure Restore() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class DirectOriginalLoadInvoker final : public IOriginalLoadInvoker {
public:
    explicit DirectOriginalLoadInvoker(std::uintptr_t originalTarget) noexcept;
    void Invoke(void* mapFile, const void* path, DWORD mode,
                DWORD validate) override;

private:
    std::uintptr_t originalTarget_;
};

}  // namespace campaign_completion
