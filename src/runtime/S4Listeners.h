#pragma once

#include "S4ModApi.h"
#include "diagnostics/Logger.h"
#include "identity/ListAttribution.h"
#include "identity/MapIdentityCoordinator.h"
#include "lua/SuLuaMapBridge.h"
#include "runtime/CallbackGate.h"
#include "runtime/ListenerRemoval.h"
#include "runtime/PageObservation.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace campaign_completion {

class RateLimiter final {
public:
    explicit RateLimiter(std::uint64_t intervalMs) : intervalMs_(intervalMs) {}

    bool Allow(std::uint64_t nowMs) noexcept {
        if (!initialized_) {
            initialized_ = true;
            lastAllowedMs_ = nowMs;
            return true;
        }
        if (nowMs - lastAllowedMs_ < intervalMs_) {
            return false;
        }
        lastAllowedMs_ = nowMs;
        return true;
    }

private:
    std::uint64_t intervalMs_;
    std::uint64_t lastAllowedMs_ = 0;
    bool initialized_ = false;
};

class S4Listeners final {
public:
    bool Start(S4API api, Logger& logger,
               MapIdentityCoordinator& coordinator,
               ILuaMapBridge& bridge);
    ListenerStopResult Stop();

private:
    class CallbackLease final {
    public:
        explicit CallbackLease(S4Listeners* owner) : owner_(owner) {
            if (owner_ == nullptr || !owner_->callbackGate_.TryEnter()) {
                owner_ = nullptr;
            }
        }
        ~CallbackLease() {
            if (owner_ != nullptr) {
                owner_->callbackGate_.Leave();
            }
        }
        explicit operator bool() const noexcept { return owner_ != nullptr; }

    private:
        S4Listeners* owner_;
    };

    template <DWORD Page>
    static HRESULT S4HCALL OnUiFrameFor(LPDIRECTDRAWSURFACE7, INT32, LPVOID) {
        DispatchUiFrame(Page);
        return S_OK;
    }

    template <std::size_t... Index>
    static constexpr auto MakeUiCallbacks(std::index_sequence<Index...>) {
        return std::array<LPS4FRAMECALLBACK, sizeof...(Index)>{
            &OnUiFrameFor<static_cast<DWORD>(Index + 1)>...};
    }

    static void DispatchUiFrame(DWORD page);
    static HRESULT S4HCALL OnMapInit(LPVOID, LPVOID);
    static HRESULT S4HCALL OnLuaOpen();
    static HRESULT S4HCALL OnTick(DWORD, BOOL, BOOL);
    static HRESULT S4HCALL OnMouse(DWORD, INT, INT, DWORD, HWND, LPCS4UIELEMENT);
    static HRESULT S4HCALL OnGuiElement(LPS4GUIDRAWBLTPARAMS, BOOL);

    void ObserveUiFrame(DWORD page);
    void ObserveMapInit();
    void ObserveLuaOpen();
    void ObserveTick(BOOL delayed);
    void ObserveMouse(DWORD button, INT x, INT y, DWORD message,
                      LPCS4UIELEMENT element);
    void ObserveGuiElement(LPS4GUIDRAWBLTPARAMS element);

    static std::atomic<S4Listeners*> active_;
    static const std::array<LPS4FRAMECALLBACK, S4_GUI_ENUM_MAXVALUE - 1>
        uiCallbacks_;
    S4API api_ = nullptr;
    Logger* logger_ = nullptr;
    MapIdentityCoordinator* coordinator_ = nullptr;
    ILuaMapBridge* bridge_ = nullptr;
    std::vector<S4HOOK> hooks_;
    CallbackGate callbackGate_;
    std::mutex mutex_;
    PageObservationWindow pageWindow_{1000};
    ListAttribution listAttribution_{kApprovedTabControls};
    RateLimiter mouseLimiter_{1000};
    RateLimiter guiLimiter_{1000};
    DWORD currentPage_ = S4_GUI_UNKNOWN;
    std::uint64_t guiElementCount_ = 0;

    bool StartPublicListeners(S4API api, Logger& logger);
};

}  // namespace campaign_completion
