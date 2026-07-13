#include "native/NativeVictoryEventSubscriber.h"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeRegistration final
    : public campaign_completion::INativeEventRegistration {
public:
    bool Register(void* handle) noexcept override {
        ++registerCalls;
        operations += "register;";
        lastRegistered = handle;
        return registerResult;
    }

    bool Unregister(void* handle) noexcept override {
        ++unregisterCalls;
        operations += "unregister;";
        lastUnregistered = handle;
        return unregisterResult;
    }

    bool registerResult = true;
    bool unregisterResult = true;
    unsigned registerCalls = 0u;
    unsigned unregisterCalls = 0u;
    std::string operations;
    void* lastRegistered = nullptr;
    void* lastUnregistered = nullptr;
};

}  // namespace

int RunNativeVictoryEventSubscriberTests() {
    using campaign_completion::DispatchNativeVictoryEvent;
    using campaign_completion::NativeEventObjectView;
    using campaign_completion::NativeLocalResult;
    using campaign_completion::NativeSubscriptionState;
    using campaign_completion::NativeVictoryEventSubscriber;
    using campaign_completion::VictoryEventProbe;

    static_assert(offsetof(NativeEventObjectView, eventId) == 0x04u);
    static_assert(offsetof(NativeEventObjectView, wParam) == 0x08u);
    static_assert(offsetof(NativeEventObjectView, lParam) == 0x0Cu);
    static_assert(offsetof(NativeEventObjectView, gameTick) == 0x10u);
    static_assert(sizeof(NativeEventObjectView) == 0x14u);

    {
        FakeRegistration registrar;
        VictoryEventProbe probe;
        probe.BeginSession(7u);
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "subscriber prepares once");
        Require(!subscriber.Prepare(registrar, probe),
                "duplicate prepare rejected");
        Require(subscriber.state() ==
                    NativeSubscriptionState::AttachRequested,
                "prepare requests attach");

        subscriber.ServiceOnGameThread();
        subscriber.ServiceOnGameThread();
        Require(registrar.registerCalls == 1u,
                "game-thread service attaches exactly once");
        Require(registrar.lastRegistered == &subscriber,
                "subscriber object is registered");
        Require(subscriber.state() == NativeSubscriptionState::Attached,
                "successful attach recorded");

        NativeEventObjectView event{};
        event.eventId = 609u;
        event.wParam = 1u;
        event.lParam = 3u;
        event.gameTick = 123u;
        Require(!subscriber.OnEvent(event),
                "native callback never consumes event");
        const auto observed = probe.ConsumePending();
        Require(observed.has_value() &&
                    observed->result == NativeLocalResult::Won &&
                    observed->fields.gameTick == 123u,
                "native callback copies terminal fields");

        subscriber.RequestDetach();
        Require(subscriber.state() ==
                    NativeSubscriptionState::DetachRequested,
                "detach requested without native call");
        subscriber.ServiceOnGameThread();
        subscriber.ServiceOnGameThread();
        Require(registrar.unregisterCalls == 1u,
                "game-thread service detaches exactly once");
        Require(registrar.lastUnregistered == &subscriber,
                "same subscriber object is unregistered");
        Require(subscriber.Detached(), "successful detach recorded");
    }

    {
        FakeRegistration registrar;
        registrar.registerResult = false;
        VictoryEventProbe probe;
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "attach failure fixture prepares");
        subscriber.ServiceOnGameThread();
        subscriber.ServiceOnGameThread();
        Require(registrar.registerCalls == 1u,
                "failed attach is not retried implicitly");
        Require(subscriber.state() == NativeSubscriptionState::AttachFailed,
                "attach failure is explicit");
        subscriber.RequestDetach();
        Require(subscriber.Detached(),
                "never-attached subscriber detaches without native call");
        Require(registrar.unregisterCalls == 0u,
                "never-attached subscriber is not unregistered");
    }

    {
        FakeRegistration registrar;
        registrar.unregisterResult = false;
        VictoryEventProbe probe;
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "detach failure fixture prepares");
        subscriber.ServiceOnGameThread();
        subscriber.RequestDetach();
        subscriber.ServiceOnGameThread();
        subscriber.ServiceOnGameThread();
        Require(registrar.unregisterCalls == 1u,
                "failed detach is not retried implicitly");
        Require(subscriber.state() == NativeSubscriptionState::DetachFailed,
                "detach failure is explicit and fail closed");
        Require(!subscriber.Detached(), "detach failure is not detached");
    }

    {
        FakeRegistration registrar;
        VictoryEventProbe probe;
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "pre-attach cancellation fixture prepares");
        subscriber.RequestDetach();
        subscriber.ServiceOnGameThread();
        Require(subscriber.Detached(), "pre-attach cancellation is detached");
        Require(registrar.registerCalls == 0u &&
                    registrar.unregisterCalls == 0u,
                "pre-attach cancellation makes no native call");
    }

    {
        FakeRegistration registrar;
        VictoryEventProbe probe;
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "reorder fixture prepares");
        subscriber.ServiceOnGameThread();
        registrar.operations.clear();

        Require(subscriber.ReinsertAtFrontOnGameThread(),
                "attached subscriber can move to the list front");
        Require(registrar.operations == "unregister;register;",
                "reorder removes the old node before pushing it to the front");
        Require(registrar.lastUnregistered == &subscriber &&
                    registrar.lastRegistered == &subscriber,
                "reorder operates on the same subscriber object");
        Require(subscriber.state() == NativeSubscriptionState::Attached,
                "successful reorder remains attached");
    }

    {
        FakeRegistration registrar;
        VictoryEventProbe probe;
        probe.BeginSession(9u);
        NativeVictoryEventSubscriber subscriber;
        Require(subscriber.Prepare(registrar, probe),
                "adapter fixture prepares");
        subscriber.ServiceOnGameThread();

        NativeEventObjectView malformed{};
        malformed.eventId = 609u;
        malformed.wParam = 7u;
        Require(!DispatchNativeVictoryEvent(&subscriber, &malformed),
                "adapter never consumes malformed event");
        Require(probe.ConsumePending()->result == NativeLocalResult::Malformed,
                "malformed terminal value is retained as evidence");
        Require(!DispatchNativeVictoryEvent(nullptr, &malformed),
                "null subscriber is transparent");
        Require(!DispatchNativeVictoryEvent(&subscriber, nullptr),
                "null event is transparent");

        probe.Disable();
        Require(!subscriber.OnEvent(malformed),
                "disabled probe remains transparent");
        Require(!probe.ConsumePending().has_value(),
                "disabled probe stores no callback evidence");
    }

    return 0;
}
