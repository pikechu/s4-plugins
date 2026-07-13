#include "runtime/PageObservation.h"

#include <algorithm>

namespace campaign_completion {

PageObservationWindow::PageObservationWindow(std::uint64_t intervalMs)
    : intervalMs_(intervalMs) {}

std::optional<PageSnapshot> PageObservationWindow::Observe(
    DWORD page, std::uint64_t nowMs) {
    if (page == S4_GUI_UNKNOWN || page >= S4_GUI_ENUM_MAXVALUE) {
        return std::nullopt;
    }

    if (!started_) {
        started_ = true;
        startedAtMs_ = nowMs;
    }
    observed_[page] = true;

    if (nowMs - startedAtMs_ < intervalMs_) {
        return std::nullopt;
    }

    PageSnapshot snapshot;
    for (DWORD candidate = 1; candidate < S4_GUI_ENUM_MAXVALUE; ++candidate) {
        if (observed_[candidate]) {
            snapshot.activePages.push_back(candidate);
        }
    }
    if (!snapshot.activePages.empty()) {
        snapshot.primaryPage = snapshot.activePages.back();
    }

    observed_.fill(false);
    startedAtMs_ = 0;
    started_ = false;
    return snapshot;
}

}  // namespace campaign_completion
