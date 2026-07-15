#pragma once

#include "runtime/PageObservation.h"

#include <array>
#include <optional>

namespace campaign_completion {

class UiPageCycleObservation final {
public:
    explicit UiPageCycleObservation(DWORD terminalPage) noexcept;

    std::optional<PageSnapshot> Observe(DWORD page) noexcept;
    void Reset() noexcept;

private:
    std::array<bool, static_cast<std::size_t>(S4_GUI_ENUM_MAXVALUE)> observed_{};
    DWORD terminalPage_ = S4_GUI_UNKNOWN;
};

}  // namespace campaign_completion
