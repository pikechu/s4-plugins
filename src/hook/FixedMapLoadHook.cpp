#include "hook/FixedMapLoadHook.h"

#include <atomic>
#include <utility>

namespace campaign_completion {
namespace {

std::atomic<FixedMapLoadHook*> g_activeHook{nullptr};

void InvokeOriginal(IOriginalLoadInvoker* original, void* mapFile,
                    const void* pathObject, DWORD mode,
                    DWORD validate) noexcept {
    if (original == nullptr) {
        return;
    }
    try {
        original->Invoke(mapFile, pathObject, mode, validate);
    } catch (...) {
    }
}

}  // namespace

bool FixedMapLoadHook::Start(HookSiteAdmission admission,
                             ICallPatchBackend& backend,
                             IOriginalLoadInvoker& original,
                             IRawCaptureSink& sink) {
    if (!admission || started_ || retired_) {
        return false;
    }

    FixedMapLoadHook* expected = nullptr;
    if (!g_activeHook.compare_exchange_strong(expected, this)) {
        return false;
    }

    backend_ = &backend;
    original_ = &original;
    sink_ = &sink;
    const auto destination = reinterpret_cast<std::uintptr_t>(
        &CampaignCompletionFixedMapAdapter);
    if (backend.Install(admission.callSite, destination,
                        kApprovedFixedMapHookSite.expectedCallBytes) !=
        PatchFailure::None) {
        g_activeHook.store(nullptr);
        return false;
    }
    if (backend.Inspect() != PatchState::InstalledByUs) {
        g_activeHook.store(nullptr);
        return false;
    }

    started_ = true;
    return true;
}

PatchFailure FixedMapLoadHook::Stop() {
    if (!started_ || backend_ == nullptr) {
        return PatchFailure::NotStarted;
    }

    gate_.CloseAndWait();
    const auto beforeRestore = backend_->Inspect();
    if (beforeRestore != PatchState::InstalledByUs) {
        return PatchFailure::Conflict;
    }
    const auto restore = backend_->Restore();
    if (restore != PatchFailure::None) {
        return restore;
    }
    if (backend_->Inspect() != PatchState::Original) {
        return PatchFailure::VerificationFailed;
    }

    FixedMapLoadHook* expected = this;
    g_activeHook.compare_exchange_strong(expected, nullptr);
    started_ = false;
    retired_ = true;
    return PatchFailure::None;
}

void FixedMapLoadHook::Dispatch(void* mapFile, const void* pathObject,
                                DWORD mode, DWORD validate) noexcept {
    const bool entered = gate_.TryEnter();
    if (entered) {
        const auto sequence = nextSequence_.fetch_add(1u);
        auto capture = CaptureMsvcX86WideString(pathObject, 512u);
        if (sink_ != nullptr) {
            try {
                sink_->Observe(std::move(capture), sequence, GetTickCount64());
            } catch (...) {
            }
        }
    }

    InvokeOriginal(original_, mapFile, pathObject, mode, validate);
    if (entered) {
        gate_.Leave();
    }
}

extern "C" __declspec(dllexport) void __fastcall
CampaignCompletionFixedMapAdapter(void* mapFile, void* ignoredEdx,
                                  const void* pathObject, DWORD mode,
                                  DWORD validate) noexcept {
    (void)ignoredEdx;
    auto* hook = g_activeHook.load();
    if (hook != nullptr) {
        hook->Dispatch(mapFile, pathObject, mode, validate);
    }
}

}  // namespace campaign_completion
