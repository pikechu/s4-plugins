#include "runtime/CallbackGate.h"

namespace campaign_completion {

bool CallbackGate::TryEnter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) {
        return false;
    }
    ++inFlight_;
    return true;
}

void CallbackGate::Leave() {
    std::lock_guard<std::mutex> lock(mutex_);
    --inFlight_;
    if (inFlight_ == 0) {
        condition_.notify_all();
    }
}

void CallbackGate::CloseAndWait() {
    std::unique_lock<std::mutex> lock(mutex_);
    accepting_ = false;
    condition_.wait(lock, [this] { return inFlight_ == 0; });
}

}  // namespace campaign_completion
