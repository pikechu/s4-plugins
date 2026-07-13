#include "native/NativeVictoryEventSubscriber.h"

#include <windows.h>

namespace campaign_completion {
namespace {

bool CopyEventFields(const NativeEventObjectView* event,
                     NativeEventFields& fields) noexcept {
    if (event == nullptr) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        fields.eventId = event->eventId;
        fields.wParam = event->wParam;
        fields.lParam = event->lParam;
        fields.gameTick = event->gameTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    fields.eventId = event->eventId;
    fields.wParam = event->wParam;
    fields.lParam = event->lParam;
    fields.gameTick = event->gameTick;
#endif
    return true;
}

}  // namespace

bool NativeVictoryEventSubscriber::Prepare(
    INativeEventRegistration& registration, VictoryEventProbe& probe) noexcept {
    auto expected = NativeSubscriptionState::Idle;
    if (!state_.compare_exchange_strong(
            expected, NativeSubscriptionState::Attaching,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }
    registration_ = &registration;
    probe_.store(&probe, std::memory_order_release);
    state_.store(NativeSubscriptionState::AttachRequested,
                 std::memory_order_release);
    return true;
}

void NativeVictoryEventSubscriber::ServiceOnGameThread() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    if (current == NativeSubscriptionState::AttachRequested) {
        if (detachRequested_.load(std::memory_order_acquire)) {
            state_.compare_exchange_strong(
                current, NativeSubscriptionState::Detached,
                std::memory_order_acq_rel, std::memory_order_acquire);
            probe_.store(nullptr, std::memory_order_release);
            return;
        }
        if (!state_.compare_exchange_strong(
                current, NativeSubscriptionState::Attaching,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
        const bool attached = registration_ != nullptr &&
                              registration_->Register(this);
        if (!attached) {
            state_.store(NativeSubscriptionState::AttachFailed,
                         std::memory_order_release);
            return;
        }
        state_.store(detachRequested_.load(std::memory_order_acquire)
                         ? NativeSubscriptionState::DetachRequested
                         : NativeSubscriptionState::Attached,
                     std::memory_order_release);
        return;
    }

    if (current == NativeSubscriptionState::DetachRequested &&
        state_.compare_exchange_strong(
            current, NativeSubscriptionState::Detaching,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        const bool detached = registration_ != nullptr &&
                              registration_->Unregister(this);
        if (detached) {
            probe_.store(nullptr, std::memory_order_release);
            state_.store(NativeSubscriptionState::Detached,
                         std::memory_order_release);
        } else {
            state_.store(NativeSubscriptionState::DetachFailed,
                         std::memory_order_release);
        }
    }
}

void NativeVictoryEventSubscriber::RequestDetach() noexcept {
    detachRequested_.store(true, std::memory_order_release);
    for (;;) {
        auto current = state_.load(std::memory_order_acquire);
        switch (current) {
            case NativeSubscriptionState::Idle:
            case NativeSubscriptionState::AttachRequested:
            case NativeSubscriptionState::AttachFailed:
                if (state_.compare_exchange_weak(
                        current, NativeSubscriptionState::Detached,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    probe_.store(nullptr, std::memory_order_release);
                    return;
                }
                break;
            case NativeSubscriptionState::Attached:
                if (state_.compare_exchange_weak(
                        current, NativeSubscriptionState::DetachRequested,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return;
                }
                break;
            case NativeSubscriptionState::Attaching:
            case NativeSubscriptionState::DetachRequested:
            case NativeSubscriptionState::Detaching:
            case NativeSubscriptionState::Detached:
            case NativeSubscriptionState::DetachFailed:
                return;
        }
    }
}

NativeSubscriptionState NativeVictoryEventSubscriber::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

bool NativeVictoryEventSubscriber::Detached() const noexcept {
    return state() == NativeSubscriptionState::Detached;
}

bool NativeVictoryEventSubscriber::OnEvent(
    const NativeEventObjectView& event) noexcept {
    try {
        if (state() != NativeSubscriptionState::Attached) {
            return false;
        }
        NativeEventFields fields{};
        if (!CopyEventFields(&event, fields)) {
            return false;
        }
        auto* const probe = probe_.load(std::memory_order_acquire);
        if (probe != nullptr) {
            static_cast<void>(probe->Observe(fields));
        }
    } catch (...) {
    }
    return false;
}

bool DispatchNativeVictoryEvent(
    NativeVictoryEventSubscriber* subscriber,
    const NativeEventObjectView* event) noexcept {
    if (subscriber == nullptr || event == nullptr) {
        return false;
    }
    try {
        return subscriber->OnEvent(*event);
    } catch (...) {
        return false;
    }
}

}  // namespace campaign_completion
