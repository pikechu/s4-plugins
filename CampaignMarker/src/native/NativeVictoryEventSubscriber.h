#pragma once

#include "native/NativeEventRegistration.h"
#include "victory/VictoryEventProbe.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace campaign_completion {

struct NativeEventObjectView final {
    std::uintptr_t vtable = 0u;
    std::uint32_t eventId = 0u;
    std::uint32_t wParam = 0u;
    std::uint32_t lParam = 0u;
    std::uint32_t gameTick = 0u;
};

static_assert(sizeof(void*) == 4u,
              "native event subscription requires a Win32 build");
static_assert(offsetof(NativeEventObjectView, eventId) == 0x04u);

enum class NativeSubscriptionState {
    Idle,
    AttachRequested,
    Attaching,
    Attached,
    DetachRequested,
    Detaching,
    Detached,
    AttachFailed,
    DetachFailed,
};

class NativeVictoryEventSubscriber final {
public:
    virtual bool OnEvent(const NativeEventObjectView& event) noexcept;

    bool Prepare(INativeEventRegistration& registration,
                 VictoryEventProbe& probe) noexcept;
    void ServiceOnGameThread() noexcept;
    bool ReinsertAtFrontOnGameThread() noexcept;
    void RequestDetach() noexcept;
    NativeSubscriptionState state() const noexcept;
    bool Detached() const noexcept;

private:
    INativeEventRegistration* registration_ = nullptr;
    std::atomic<VictoryEventProbe*> probe_{nullptr};
    std::atomic<NativeSubscriptionState> state_{NativeSubscriptionState::Idle};
    std::atomic<bool> detachRequested_{false};
};

bool DispatchNativeVictoryEvent(
    NativeVictoryEventSubscriber* subscriber,
    const NativeEventObjectView* event) noexcept;

}  // namespace campaign_completion
