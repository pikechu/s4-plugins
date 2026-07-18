#pragma once

#include "completion/CompletionCandidateCoordinator.h"

#include <cstdint>
#include <functional>
#include <mutex>

namespace campaign_completion {

class CompletionAdmission final {
public:
    using Submit = std::function<bool(CompletionCandidate)>;

    CompletionAdmission(CompletionCandidateCoordinator& coordinator,
                        Submit submit) noexcept;
    void BeginSession(std::uint64_t sessionId,
                      LaunchOriginSnapshot origin) noexcept;
    bool ObserveIdentity(const ConfirmedMapIdentity& identity,
                         LaunchOriginSnapshot refinedOrigin) noexcept;
    bool ObserveVictory(const VictoryEventSnapshot& victory) noexcept;
    void Disable() noexcept;

private:
    bool SubmitCandidate(
        std::optional<CompletionCandidate> candidate) noexcept;

    CompletionCandidateCoordinator& coordinator_;
    Submit submit_;
    std::mutex mutex_;
    bool enabled_ = true;
};

}  // namespace campaign_completion
