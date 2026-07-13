#include "runtime/S4Listeners.h"

#include <windows.h>

#include <sstream>

namespace campaign_completion {

std::atomic<S4Listeners*> S4Listeners::active_{nullptr};
const std::array<LPS4FRAMECALLBACK, S4_GUI_ENUM_MAXVALUE - 1>
    S4Listeners::uiCallbacks_ = S4Listeners::MakeUiCallbacks(
        std::make_index_sequence<S4_GUI_ENUM_MAXVALUE - 1>{});

bool S4Listeners::Start(S4API api, Logger& logger,
                        FixedMapIdentityProbe& probe) {
    if (api == nullptr || active_.load() != nullptr) {
        return false;
    }
    api_ = api;
    logger_ = &logger;
    probe_ = &probe;
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
    probe_ = nullptr;
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
    std::lock_guard<std::mutex> lock(mutex_);
    const auto snapshot = pageWindow_.Observe(page, now);
    if (!snapshot.has_value() || logger_ == nullptr) {
        return;
    }
    currentPage_ = snapshot->primaryPage;
    listAttribution_.ObservePages(snapshot.value());
    if (probe_ != nullptr && snapshot->activePages.size() == 1u) {
        probe_->ObserveTopLevelPage(snapshot->activePages.front());
    }
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
    if (probe_ != nullptr) {
        probe_->ObserveMapInit(GetTickCount64());
    }
    if (logger_ != nullptr) {
        logger_->Write(LogLevel::Info, "map-init observed");
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
        if (probe_ != nullptr && message == WM_LBUTTONUP &&
            element->id == 2415u && wasFixedMap) {
            probe_->ObserveBack();
        }
        listAttribution_.ObserveClick(message, element->id);
        const auto listKind = listAttribution_.Current();
        if (message == WM_LBUTTONUP &&
            listKind != FixedMapListKind::Unknown) {
            if (probe_ != nullptr) {
                probe_->ObserveListKind(listKind, now);
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
