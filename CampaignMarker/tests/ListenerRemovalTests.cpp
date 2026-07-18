#include "runtime/ListenerRemoval.h"

#include <stdexcept>
#include <string>
#include <numeric>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunListenerRemovalTests() {
    std::vector<S4HOOK> hooks{10, 20, 30};
    std::vector<S4HOOK> attempts;
    const auto success = campaign_completion::RemoveListenersInReverse(
        hooks, [&attempts](S4HOOK hook) {
            attempts.push_back(hook);
            return S_OK;
        });
    Require(attempts == std::vector<S4HOOK>({30, 20, 10}),
            "listeners must be removed in reverse registration order");
    Require(success.registered == 3 && success.removed == 3 &&
                success.failures == 0,
            "successful removal counts must be exact");
    Require(hooks.empty(), "successful removal must clear stored handles");

    hooks = {10, 20, 30};
    attempts.clear();
    const auto partial = campaign_completion::RemoveListenersInReverse(
        hooks, [&attempts](S4HOOK hook) {
            attempts.push_back(hook);
            return hook == 20 ? E_FAIL : S_OK;
        });
    Require(attempts == std::vector<S4HOOK>({30, 20, 10}),
            "one failure must not stop remaining removal attempts");
    Require(partial.registered == 3 && partial.removed == 2 &&
                partial.failures == 1,
            "partial removal counts must be exact");
    Require(hooks.empty(), "partial removal must still clear stored handles");

    hooks.resize(77u);
    std::iota(hooks.begin(), hooks.end(), static_cast<S4HOOK>(1));
    attempts.clear();
    const auto publicListenerSet =
        campaign_completion::RemoveListenersInReverse(
            hooks, [&attempts](S4HOOK hook) {
                attempts.push_back(hook);
                return S_OK;
            });
    Require(attempts.size() == 77u && attempts.front() == 77u &&
                attempts.back() == 1u,
            "all 77 public listeners are removed in exact reverse order");
    Require(publicListenerSet.registered == 77u &&
                publicListenerSet.removed == 77u &&
                publicListenerSet.failures == 0u,
            "77-listener lifecycle counts are exact");
    Require(hooks.empty(), "77-listener removal clears every handle");
    return 0;
}
