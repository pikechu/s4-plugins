#pragma once

namespace campaign_completion {

class CompletionManagerHotkey final {
public:
    bool Observe(bool controlDown, bool shiftDown, bool mDown,
                 bool mainMenuAvailable) noexcept {
        const bool edge = mDown && !mWasDown_;
        mWasDown_ = mDown;
        return edge && controlDown && shiftDown && mainMenuAvailable;
    }

    void Reset() noexcept { mWasDown_ = false; }

private:
    bool mWasDown_ = false;
};

}  // namespace campaign_completion
