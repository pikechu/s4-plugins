#include "runtime/S4Listeners.h"

#include <windows.h>

#include <sstream>
#include <string>

namespace campaign_completion {
namespace {

std::string SettlementFeatureRecord(const SettlementFeature& feature) {
    std::ostringstream output;
    output << "settlement-feature=gfx-" << feature.gfxCollection
           << ";container-" << feature.containerType << ";x-" << feature.x
           << ";y-" << feature.y << ";width-" << feature.width
           << ";height-" << feature.height << ";main-texture-"
           << feature.mainTexture << ";value-link-" << feature.valueLink
           << ";pressed-texture-" << feature.buttonPressedTexture
           << ";tooltip-link-" << feature.tooltipLink << ";image-style-"
           << feature.imageStyle << ";effects-" << feature.effects
           << ";text-style-" << feature.textStyle << ";show-texture-"
           << feature.showTexture << ";back-texture-" << feature.backTexture;
    return output.str();
}

const char* NativeStateName(NativeSubscriptionState state) noexcept {
    switch (state) {
        case NativeSubscriptionState::Idle:
            return "idle";
        case NativeSubscriptionState::AttachRequested:
            return "attach-requested";
        case NativeSubscriptionState::Attaching:
            return "attaching";
        case NativeSubscriptionState::Attached:
            return "attached";
        case NativeSubscriptionState::DetachRequested:
            return "detach-requested";
        case NativeSubscriptionState::Detaching:
            return "detaching";
        case NativeSubscriptionState::Detached:
            return "detached";
        case NativeSubscriptionState::AttachFailed:
            return "attach-failed";
        case NativeSubscriptionState::DetachFailed:
            return "detach-failed";
    }
    return "unknown";
}

const char* NativeResultName(NativeLocalResult result) noexcept {
    switch (result) {
        case NativeLocalResult::None:
            return "none";
        case NativeLocalResult::Won:
            return "won";
        case NativeLocalResult::Lost:
            return "lost";
        case NativeLocalResult::Malformed:
            return "malformed";
        case NativeLocalResult::Conflict:
            return "conflict";
    }
    return "unknown";
}

}  // namespace

std::atomic<S4Listeners*> S4Listeners::active_{nullptr};
const std::array<LPS4FRAMECALLBACK, S4_GUI_ENUM_MAXVALUE - 1>
    S4Listeners::uiCallbacks_ = S4Listeners::MakeUiCallbacks(
        std::make_index_sequence<S4_GUI_ENUM_MAXVALUE - 1>{});

bool S4Listeners::Start(S4API api, Logger& logger,
                        MapIdentityCoordinator& coordinator,
                        ILuaMapBridge& bridge,
                        LaunchOriginTracker& origin,
                        SettlementUiProbe& settlement,
                        NativeVictoryEventSubscriber& subscriber,
                        VictoryEventProbe& victoryProbe,
                        Phase3Trace& phase3Trace) {
    coordinator_ = &coordinator;
    bridge_ = &bridge;
    origin_ = &origin;
    settlement_ = &settlement;
    nativeSubscriber_ = &subscriber;
    victoryProbe_ = &victoryProbe;
    phase3Trace_ = &phase3Trace;
    return StartPublicListeners(api, logger);
}

bool S4Listeners::StartPublicListeners(S4API api, Logger& logger) {
    if (api == nullptr || active_.load() != nullptr) {
        return false;
    }
    api_ = api;
    logger_ = &logger;
    active_.store(this);

    const auto addHook = [this](S4HOOK hook) {
        if (hook != 0) {
            hooks_.push_back(hook);
            return true;
        }
        return false;
    };

    bool success = addHook(api_->AddMapInitListener(&OnMapInit));
    for (DWORD page = 1; page < S4_GUI_ENUM_MAXVALUE; ++page) {
        success = addHook(api_->AddUIFrameListener(
                      uiCallbacks_[page - 1], static_cast<S4_GUI_ENUM>(page))) &&
                  success;
    }
    success = addHook(api_->AddMouseListener(&OnMouse)) && success;
    success = addHook(api_->AddGuiElementBltListener(&OnGuiElement)) && success;
    success = addHook(api_->AddLuaOpenListener(&OnLuaOpen)) && success;
    success = addHook(api_->AddTickListener(&OnTick)) && success;
    if (!success) {
        logger_->Write(LogLevel::Error, "one or more public listeners failed to register");
        Stop();
    }
    return success;
}

ListenerStopResult S4Listeners::Stop() {
    if (active_.load() == this) {
        active_.store(nullptr);
    }
    callbackGate_.CloseAndWait();
    const auto result = RemoveListenersInReverse(
        hooks_, [this](S4HOOK hook) {
            return api_ != nullptr ? api_->RemoveListener(hook) : E_POINTER;
        });
    api_ = nullptr;
    logger_ = nullptr;
    coordinator_ = nullptr;
    bridge_ = nullptr;
    origin_ = nullptr;
    settlement_ = nullptr;
    nativeSubscriber_ = nullptr;
    victoryProbe_ = nullptr;
    phase3Trace_ = nullptr;
    activeOrigin_ = {};
    activeSessionId_ = 0u;
    inGameSeen_ = false;
    settlementStarted_ = false;
    nativeReinsertPending_ = false;
    lastNativeState_.store(NativeSubscriptionState::Idle,
                           std::memory_order_release);
    return result;
}

void S4Listeners::DispatchUiFrame(DWORD page) {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveUiFrame(page);
    }
}

HRESULT S4HCALL S4Listeners::OnMapInit(LPVOID, LPVOID) {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveMapInit();
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnLuaOpen() {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveLuaOpen();
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnTick(DWORD, BOOL, BOOL delayed) {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveTick(delayed);
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnMouse(DWORD button, INT x, INT y, DWORD message,
                                     HWND, LPCS4UIELEMENT element) {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveMouse(button, x, y, message, element);
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnGuiElement(LPS4GUIDRAWBLTPARAMS element, BOOL) {
    auto* active = active_.load();
    CallbackLease lease(active);
    if (lease) {
        active->ObserveGuiElement(element);
    }
    return S_OK;
}

void S4Listeners::ObserveUiFrame(DWORD page) {
    const auto now = GetTickCount64();
    ServiceNativeSubscription();
    std::lock_guard<std::mutex> lock(mutex_);
    if (origin_ != nullptr) {
        origin_->ObservePage(page, now);
    }
    if (page == S4_SCREEN_INGAME) {
        inGameSeen_ = true;
    } else if (page == S4_SCREEN_AFTERGAME_SUMMARY && inGameSeen_ &&
               !settlementStarted_ && activeSessionId_ != 0u &&
               settlement_ != nullptr) {
        settlementStarted_ = settlement_->Begin(activeSessionId_, now);
    }
    FinishSettlementIfDue(now);

    const auto snapshot = pageWindow_.Observe(page, now);
    if (!snapshot.has_value() || logger_ == nullptr) {
        return;
    }
    currentPage_ = snapshot->primaryPage;
    listAttribution_.ObservePages(snapshot.value());
    std::ostringstream message;
    message << "ui-pages active=";
    for (std::size_t index = 0; index < snapshot->activePages.size(); ++index) {
        if (index != 0) {
            message << ',';
        }
        message << snapshot->activePages[index];
    }
    message << " primary=" << currentPage_;
    logger_->Write(LogLevel::Info, message.str());
}

void S4Listeners::ObserveMapInit() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = GetTickCount64();
    activeOrigin_ = origin_ != nullptr ? origin_->ConsumeMapInit(now)
                                       : LaunchOriginSnapshot{};
    if (coordinator_ != nullptr) {
        activeSessionId_ = coordinator_->ObserveMapInit(now);
    } else {
        activeSessionId_ = 0u;
    }
    inGameSeen_ = false;
    settlementStarted_ = false;
    nativeReinsertPending_ = true;
    if (victoryProbe_ != nullptr) {
        victoryProbe_->BeginSession(activeSessionId_);
    }
    if (phase3Trace_ != nullptr) {
        phase3Trace_->Write(
            Phase3TraceChannel::Origin,
            "origin-source=" +
                std::string(LaunchSourceName(activeOrigin_.source)));
        phase3Trace_->Write(
            Phase3TraceChannel::Origin,
            "origin-eligibility=" + std::string(
                                        SessionEligibilityName(
                                            activeOrigin_.eligibility)));
        phase3Trace_->Write(
            Phase3TraceChannel::Origin,
            "origin-status=map-init-session-" +
                std::to_string(activeSessionId_));
    }
    if (logger_ != nullptr) {
        logger_->Write(LogLevel::Info, "map-init observed");
    }
}

void S4Listeners::ObserveLuaOpen() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (coordinator_ != nullptr) {
        coordinator_->ObserveLuaOpen(GetTickCount64());
    }
}

void S4Listeners::ObserveTick(BOOL delayed) {
    if (delayed != FALSE) {
        return;
    }
    ServiceNativeSubscription();
    std::lock_guard<std::mutex> lock(mutex_);
    if (api_ == nullptr) {
        return;
    }
    const bool inGame =
        api_->IsCurrentlyOnScreen(S4_SCREEN_INGAME) != FALSE;
    const auto now = GetTickCount64();
    if (inGame && nativeReinsertPending_ && nativeSubscriber_ != nullptr &&
        nativeSubscriber_->state() == NativeSubscriptionState::Attached) {
        nativeReinsertPending_ = false;
        const bool reinserted =
            nativeSubscriber_->ReinsertAtFrontOnGameThread();
        if (phase3Trace_ != nullptr) {
            phase3Trace_->Write(
                Phase3TraceChannel::NativeEvent,
                reinserted ? "native-subscription=reinserted-front"
                           : "native-subscription=reinsert-front-failed");
        }
        if (logger_ != nullptr) {
            logger_->Write(
                reinserted ? LogLevel::Info : LogLevel::Error,
                reinserted ? "native subscriber reinserted at list front"
                           : "native subscriber list reorder failed");
        }
    }
    if (coordinator_ != nullptr && bridge_ != nullptr) {
        coordinator_->ObserveTick(inGame, now, *bridge_);
    }
    FinishSettlementIfDue(now);
}

void S4Listeners::ServiceNativeSubscription() {
    if (nativeSubscriber_ == nullptr || victoryProbe_ == nullptr) {
        return;
    }
    nativeSubscriber_->ServiceOnGameThread();

    const auto current = nativeSubscriber_->state();
    const auto previous = lastNativeState_.exchange(
        current, std::memory_order_acq_rel);
    if (current != previous && phase3Trace_ != nullptr) {
        phase3Trace_->Write(
            Phase3TraceChannel::NativeEvent,
            "native-subscription=" + std::string(NativeStateName(current)));
    }

    const auto pending = victoryProbe_->ConsumePending();
    if (!pending.has_value() || phase3Trace_ == nullptr) {
        return;
    }
    const auto& snapshot = pending.value();
    std::ostringstream event;
    event << "native-event=session-" << snapshot.sessionId
          << ";event-id=" << snapshot.fields.eventId
          << ";local-result=" << NativeResultName(snapshot.result)
          << ";wparam=" << snapshot.fields.wParam
          << ";lparam=" << snapshot.fields.lParam
          << ";game-tick=" << snapshot.fields.gameTick;
    phase3Trace_->Write(Phase3TraceChannel::NativeEvent, event.str());

    phase3Trace_->Write(
        Phase3TraceChannel::NativeEvent,
        "native-event-duplicates=session-" +
            std::to_string(snapshot.sessionId) +
            ";count=" + std::to_string(snapshot.duplicates));
    if (snapshot.orphans != 0u) {
        phase3Trace_->Write(Phase3TraceChannel::NativeEvent,
                            "native-event-orphans=count=" +
                                std::to_string(snapshot.orphans));
    }
}

void S4Listeners::FinishSettlementIfDue(std::uint64_t nowMs) {
    if (api_ == nullptr || settlement_ == nullptr || phase3Trace_ == nullptr) {
        return;
    }
    const auto capture = settlement_->FinishIfDue(nowMs);
    if (!capture.has_value()) {
        return;
    }

    const DWORD localPlayer = api_->GetLocalPlayer();
    phase3Trace_->Write(
        Phase3TraceChannel::SettlementUi,
        localPlayer == 0u ? "local-player-status=unavailable"
                          : "local-player-status=available");
    if (localPlayer != 0u) {
        const BOOL lost = api_->HasPlayerLost(localPlayer);
        phase3Trace_->Write(Phase3TraceChannel::SettlementUi,
                            lost != FALSE ? "local-player-lost=1"
                                          : "local-player-lost=0");
    }

    std::ostringstream summary;
    summary << "settlement-capture=session-" << capture->sessionId
            << ";features=" << capture->features.size()
            << ";truncated=" << (capture->truncated ? 1 : 0);
    phase3Trace_->Write(Phase3TraceChannel::SettlementUi, summary.str());
    for (const auto& feature : capture->features) {
        phase3Trace_->Write(Phase3TraceChannel::SettlementUi,
                            SettlementFeatureRecord(feature));
    }
    phase3Trace_->Write(Phase3TraceChannel::Decision,
                        "diagnostic-result=calibration-only");
    if (logger_ != nullptr) {
        logger_->Write(LogLevel::Info, "settlement UI calibration captured");
    }
}

void S4Listeners::ObserveMouse(DWORD button, INT x, INT y, DWORD message,
                               LPCS4UIELEMENT element) {
    const auto now = GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_ == nullptr) {
        return;
    }
    if (element != nullptr) {
        const bool wasFixedMap =
            listAttribution_.Current() != FixedMapListKind::Unknown;
        if (origin_ != nullptr && message == WM_LBUTTONUP &&
            element->id == 2415u) {
            origin_->ObserveBack();
        }
        if (coordinator_ != nullptr && message == WM_LBUTTONUP &&
            element->id == 2415u && wasFixedMap) {
            coordinator_->ObserveBack();
        }
        listAttribution_.ObserveClick(message, element->id);
        const auto listKind = listAttribution_.Current();
        if (message == WM_LBUTTONUP &&
            listKind != FixedMapListKind::Unknown) {
            if (coordinator_ != nullptr) {
                coordinator_->ObserveListKind(listKind, now);
            }
            if (origin_ != nullptr) {
                origin_->ObserveListKind(listKind, now);
            }
            std::ostringstream attribution;
            attribution << "fixed-map-list list_kind="
                        << FixedMapListKindName(listKind)
                        << " element-id=" << element->id
                        << " active=4,22,23,25";
            logger_->Write(LogLevel::Info, attribution.str());
        }
    }
    if (!mouseLimiter_.Allow(now)) {
        return;
    }
    std::ostringstream output;
    output << "mouse page=" << currentPage_ << " button=" << button
           << " message=" << message << " x=" << x << " y=" << y;
    if (element != nullptr) {
        output << " element-id=" << element->id << " rect=" << element->x << ','
               << element->y << ',' << element->w << ',' << element->h;
    }
    logger_->Write(LogLevel::Info, output.str());
}

void S4Listeners::ObserveGuiElement(LPS4GUIDRAWBLTPARAMS element) {
    const auto now = GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex_);
    ++guiElementCount_;
    if (element != nullptr && settlement_ != nullptr &&
        settlement_->Active()) {
        SettlementFeature feature{};
        feature.gfxCollection = element->currentGFXCollection;
        feature.containerType = element->containerType;
        feature.x = element->x;
        feature.y = element->y;
        feature.width = element->width;
        feature.height = element->height;
        feature.mainTexture = element->mainTexture;
        feature.valueLink = element->valueLink;
        feature.buttonPressedTexture = element->buttonPressedTexture;
        feature.tooltipLink = element->tooltipLink;
        feature.imageStyle = static_cast<std::uint16_t>(element->imageStyle);
        feature.effects = static_cast<std::uint16_t>(element->effects);
        feature.textStyle = static_cast<std::uint16_t>(element->textStyle);
        feature.showTexture = element->showTexture;
        feature.backTexture = element->backTexture;
        settlement_->Observe(feature);
    }
    if (!guiLimiter_.Allow(now) || logger_ == nullptr) {
        return;
    }
    std::ostringstream output;
    output << "gui-elements page=" << currentPage_ << " count=" << guiElementCount_;
    if (element != nullptr) {
        output << " last-rect=" << element->x << ',' << element->y << ','
               << element->width << ',' << element->height
               << " value-link=" << element->valueLink
               << " texture=" << element->mainTexture;
    }
    guiElementCount_ = 0;
    logger_->Write(LogLevel::Info, output.str());
}

}  // namespace campaign_completion
