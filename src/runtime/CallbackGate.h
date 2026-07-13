#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace campaign_completion {

class CallbackGate final {
public:
    bool TryEnter();
    void Leave();
    void CloseAndWait();

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t inFlight_ = 0;
    bool accepting_ = true;
};

}  // namespace campaign_completion
