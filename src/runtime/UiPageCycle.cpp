#include "runtime/UiPageCycle.h"

namespace campaign_completion {

UiPageCycleObservation::UiPageCycleObservation(
    DWORD terminalPage) noexcept
    : terminalPage_(terminalPage) {}

std::optional<PageSnapshot> UiPageCycleObservation::Observe(
    DWORD page) noexcept {
    if (page == S4_GUI_UNKNOWN || page >= S4_GUI_ENUM_MAXVALUE ||
        terminalPage_ == S4_GUI_UNKNOWN ||
        terminalPage_ >= S4_GUI_ENUM_MAXVALUE) {
        return std::nullopt;
    }
    observed_[page] = true;
    if (page != terminalPage_) return std::nullopt;

    try {
        PageSnapshot snapshot{};
        for (DWORD candidate = 1u; candidate < S4_GUI_ENUM_MAXVALUE;
             ++candidate) {
            if (observed_[candidate]) snapshot.activePages.push_back(candidate);
        }
        if (!snapshot.activePages.empty()) {
            snapshot.primaryPage = snapshot.activePages.back();
        }
        Reset();
        return snapshot;
    } catch (...) {
        Reset();
        return std::nullopt;
    }
}

void UiPageCycleObservation::Reset() noexcept {
    observed_.fill(false);
}

}  // namespace campaign_completion
