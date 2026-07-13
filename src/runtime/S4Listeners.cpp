#include "runtime/S4Listeners.h"

#include <windows.h>

#include <sstream>

namespace campaign_completion {

std::atomic<S4Listeners*> S4Listeners::active_{nullptr};

bool S4Listeners::Start(S4API api, Logger& logger) {
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
                      &OnUiFrame, static_cast<S4_GUI_ENUM>(page))) &&
                  success;
    }
    success = addHook(api_->AddMouseListener(&OnMouse)) && success;
    success = addHook(api_->AddGuiElementBltListener(&OnGuiElement)) && success;
    if (!success) {
        logger_->Write(LogLevel::Error, "one or more public listeners failed to register");
        Stop();
    }
    return success;
}

void S4Listeners::Stop() {
    if (api_ != nullptr) {
        for (auto hook = hooks_.rbegin(); hook != hooks_.rend(); ++hook) {
            if (FAILED(api_->RemoveListener(*hook)) && logger_ != nullptr) {
                logger_->Write(LogLevel::Warning, "RemoveListener failed");
            }
        }
    }
    hooks_.clear();
    if (active_.load() == this) {
        active_.store(nullptr);
    }
    api_ = nullptr;
    logger_ = nullptr;
}

HRESULT S4HCALL S4Listeners::OnUiFrame(LPDIRECTDRAWSURFACE7, INT32, LPVOID) {
    if (auto* active = active_.load(); active != nullptr) {
        active->ObserveUiFrame();
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnMapInit(LPVOID, LPVOID) {
    if (auto* active = active_.load(); active != nullptr) {
        active->ObserveMapInit();
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnMouse(DWORD button, INT x, INT y, DWORD message,
                                     HWND, LPCS4UIELEMENT element) {
    if (auto* active = active_.load(); active != nullptr) {
        active->ObserveMouse(button, x, y, message, element);
    }
    return S_OK;
}

HRESULT S4HCALL S4Listeners::OnGuiElement(LPS4GUIDRAWBLTPARAMS element, BOOL) {
    if (auto* active = active_.load(); active != nullptr) {
        active->ObserveGuiElement(element);
    }
    return S_OK;
}

void S4Listeners::ObserveUiFrame() {
    const auto now = GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pageLimiter_.Allow(now) || api_ == nullptr || logger_ == nullptr) {
        return;
    }
    currentPage_ = S4_GUI_UNKNOWN;
    for (DWORD page = 1; page < S4_GUI_ENUM_MAXVALUE; ++page) {
        if (api_->IsCurrentlyOnScreen(static_cast<S4_GUI_ENUM>(page)) != FALSE) {
            currentPage_ = page;
            break;
        }
    }
    std::ostringstream message;
    message << "ui-frame page=" << currentPage_;
    logger_->Write(LogLevel::Info, message.str());
}

void S4Listeners::ObserveMapInit() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_ != nullptr) {
        logger_->Write(LogLevel::Info, "map-init observed");
    }
}

void S4Listeners::ObserveMouse(DWORD button, INT x, INT y, DWORD message,
                               LPCS4UIELEMENT element) {
    const auto now = GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex_);
    if (!mouseLimiter_.Allow(now) || logger_ == nullptr) {
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
