#pragma once

#include "S4ModApi.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace campaign_completion {

struct PageSnapshot final {
    std::vector<DWORD> activePages;
    DWORD primaryPage = S4_GUI_UNKNOWN;
};

class PageObservationWindow final {
public:
    explicit PageObservationWindow(std::uint64_t intervalMs);
    std::optional<PageSnapshot> Observe(DWORD page, std::uint64_t nowMs);

private:
    std::array<bool, static_cast<std::size_t>(S4_GUI_ENUM_MAXVALUE)> observed_{};
    std::uint64_t intervalMs_;
    std::uint64_t startedAtMs_ = 0;
    bool started_ = false;
};

}  // namespace campaign_completion
