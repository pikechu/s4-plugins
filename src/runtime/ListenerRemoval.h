#pragma once

#include "S4ModApi.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace campaign_completion {

struct ListenerStopResult final {
    std::size_t registered = 0;
    std::size_t removed = 0;
    std::size_t failures = 0;
};

using ListenerRemover = std::function<HRESULT(S4HOOK)>;

ListenerStopResult RemoveListenersInReverse(std::vector<S4HOOK>& hooks,
                                            const ListenerRemover& remover);

}  // namespace campaign_completion
