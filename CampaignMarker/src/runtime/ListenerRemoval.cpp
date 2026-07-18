#include "runtime/ListenerRemoval.h"

namespace campaign_completion {

ListenerStopResult RemoveListenersInReverse(std::vector<S4HOOK>& hooks,
                                            const ListenerRemover& remover) {
    ListenerStopResult result;
    result.registered = hooks.size();
    for (auto hook = hooks.rbegin(); hook != hooks.rend(); ++hook) {
        if (SUCCEEDED(remover(*hook))) {
            ++result.removed;
        } else {
            ++result.failures;
        }
    }
    hooks.clear();
    return result;
}

}  // namespace campaign_completion
