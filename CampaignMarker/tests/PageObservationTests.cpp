#include "runtime/PageObservation.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunPageObservationTests() {
    campaign_completion::PageObservationWindow window(1000);
    Require(!window.Observe(4, 0).has_value(), "parent starts window");
    Require(!window.Observe(22, 10).has_value(), "child joins window");
    Require(!window.Observe(22, 999).has_value(), "duplicate before deadline");

    const auto snapshot = window.Observe(22, 1000);
    Require(snapshot.has_value(), "deadline produces snapshot");
    Require(snapshot->activePages == std::vector<DWORD>({4, 22}),
            "sorted unique pages");
    Require(snapshot->primaryPage == 22, "specific child is primary");
    Require(!window.Observe(1, 1001).has_value(), "window resets");
    return 0;
}
