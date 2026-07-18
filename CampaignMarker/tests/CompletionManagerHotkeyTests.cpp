#include "manager/CompletionManagerHotkey.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int RunCompletionManagerHotkeyTests() {
    campaign_completion::CompletionManagerHotkey hotkey;
    Require(!hotkey.Observe(true, true, true, false),
            "hotkey must be main-menu gated");
    Require(!hotkey.Observe(true, true, true, true),
            "holding M across a page transition must not open");
    Require(!hotkey.Observe(true, true, false, true),
            "release only rearms");
    Require(hotkey.Observe(true, true, true, true),
            "fresh Ctrl+Shift+M edge opens once");
    Require(!hotkey.Observe(true, true, true, true),
            "held hotkey never repeats");
    Require(!hotkey.Observe(false, true, false, true) &&
                !hotkey.Observe(false, true, true, true),
            "missing modifier does not open");
    return 0;
}
