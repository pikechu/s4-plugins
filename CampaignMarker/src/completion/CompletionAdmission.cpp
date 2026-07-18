#include "completion/CompletionAdmission.h"

#include <utility>

namespace campaign_completion {

CompletionAdmission::CompletionAdmission(
    CompletionCandidateCoordinator& coordinator, Submit submit) noexcept
    : coordinator_(coordinator), submit_(std::move(submit)) {}

void CompletionAdmission::BeginSession(
    std::uint64_t sessionId, LaunchOriginSnapshot origin) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (enabled_) {
            coordinator_.BeginSession(sessionId, origin);
        }
    } catch (...) {
    }
}

bool CompletionAdmission::ObserveIdentity(
    const ConfirmedMapIdentity& identity,
    LaunchOriginSnapshot refinedOrigin) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return false;
        }
        return SubmitCandidate(
            coordinator_.ObserveIdentity(identity, refinedOrigin));
    } catch (...) {
        return false;
    }
}

bool CompletionAdmission::ObserveVictory(
    const VictoryEventSnapshot& victory) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return false;
        }
        return SubmitCandidate(coordinator_.ObserveVictory(victory));
    } catch (...) {
        return false;
    }
}

void CompletionAdmission::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        coordinator_.Disable();
    } catch (...) {
    }
}

bool CompletionAdmission::SubmitCandidate(
    std::optional<CompletionCandidate> candidate) noexcept {
    if (!candidate.has_value() || !submit_) {
        return false;
    }
    try {
        return submit_(std::move(candidate.value()));
    } catch (...) {
        return false;
    }
}

}  // namespace campaign_completion
