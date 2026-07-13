#pragma once

#include "S4ModApi.h"
#include "diagnostics/Logger.h"

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
    bool Start(S4API api, Logger& logger);
    void Stop();

private:
    static HRESULT S4HCALL OnUiFrame(LPDIRECTDRAWSURFACE7, INT32, LPVOID);
    static HRESULT S4HCALL OnMapInit(LPVOID, LPVOID);
    static HRESULT S4HCALL OnMouse(DWORD, INT, INT, DWORD, HWND, LPCS4UIELEMENT);
    static HRESULT S4HCALL OnGuiElement(LPS4GUIDRAWBLTPARAMS, BOOL);

    void ObserveUiFrame();
    void ObserveMapInit();
    void ObserveMouse(DWORD button, INT x, INT y, DWORD message,
                      LPCS4UIELEMENT element);
    void ObserveGuiElement(LPS4GUIDRAWBLTPARAMS element);

    static std::atomic<S4Listeners*> active_;
    S4API api_ = nullptr;
    Logger* logger_ = nullptr;
    std::vector<S4HOOK> hooks_;
    std::mutex mutex_;
    RateLimiter pageLimiter_{1000};
    RateLimiter mouseLimiter_{1000};
    RateLimiter guiLimiter_{1000};
    DWORD currentPage_ = S4_GUI_UNKNOWN;
    std::uint64_t guiElementCount_ = 0;
};

}  // namespace campaign_completion
