#pragma once

#include "hook/CallPatchBackend.h"
#include "hook/HookSiteLayout.h"
#include "hook/MsvcX86WideString.h"
#include "runtime/CallbackGate.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

namespace campaign_completion {

class IOriginalLoadInvoker {
public:
    virtual ~IOriginalLoadInvoker() = default;
    virtual void Invoke(void* mapFile, const void* path, DWORD mode,
                        DWORD validate) = 0;
};

class IRawCaptureSink {
public:
    virtual ~IRawCaptureSink() = default;
    virtual void Observe(CapturedWidePath capture, std::uint64_t sequence,
                         std::uint64_t nowMs) = 0;
};

class FixedMapLoadHook final {
public:
    bool Start(HookSiteAdmission admission, ICallPatchBackend& backend,
               IOriginalLoadInvoker& original, IRawCaptureSink& sink);
    PatchFailure Stop();

    void Dispatch(void* mapFile, const void* pathObject, DWORD mode,
                  DWORD validate) noexcept;

private:
    CallbackGate gate_;
    ICallPatchBackend* backend_ = nullptr;
    IOriginalLoadInvoker* original_ = nullptr;
    IRawCaptureSink* sink_ = nullptr;
    std::atomic<std::uint64_t> nextSequence_{1u};
    bool started_ = false;
    bool retired_ = false;
};

extern "C" __declspec(dllexport) void __fastcall
CampaignCompletionFixedMapAdapter(void* mapFile, void* ignoredEdx,
                                  const void* pathObject, DWORD mode,
                                  DWORD validate) noexcept;

}  // namespace campaign_completion
