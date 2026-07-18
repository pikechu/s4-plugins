#include "runtime/UiPageCycle.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int RunUiPageCycleTests() {
    using namespace campaign_completion;

    UiPageCycleObservation cycle(25u);
    Require(!cycle.Observe(4u).has_value() &&
                !cycle.Observe(22u).has_value() &&
                !cycle.Observe(23u).has_value(),
            "a page cycle does not publish before its terminal layer");
    const auto exact = cycle.Observe(25u);
    Require(exact.has_value() &&
                exact->activePages == std::vector<DWORD>({4u, 22u, 23u, 25u}) &&
                exact->primaryPage == 25u,
            "page 25 closes one exact sorted fixed-map render cycle");

    Require(!cycle.Observe(1u).has_value() &&
                !cycle.Observe(7u).has_value() &&
                !cycle.Observe(4u).has_value() &&
                !cycle.Observe(22u).has_value() &&
                !cycle.Observe(23u).has_value(),
            "transition pages accumulate until the terminal layer");
    const auto transition = cycle.Observe(25u);
    Require(transition.has_value() && transition->activePages.size() == 6u &&
                transition->activePages.front() == 1u,
            "a transition cycle exposes its extra pages for fail-closed gating");

    const auto terminalOnly = cycle.Observe(25u);
    Require(terminalOnly.has_value() &&
                terminalOnly->activePages == std::vector<DWORD>({25u}),
            "each terminal layer resets the prior cycle atomically");

    cycle.Observe(4u);
    cycle.Reset();
    const auto afterReset = cycle.Observe(25u);
    Require(afterReset.has_value() && afterReset->activePages.size() == 1u,
            "explicit reset discards partial page evidence");
    return 0;
}
