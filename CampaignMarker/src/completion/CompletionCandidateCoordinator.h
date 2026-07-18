#pragma once

#include "completion/CompletionRecord.h"
#include "identity/MapIdentityCoordinator.h"
#include "victory/LaunchOrigin.h"
#include "victory/VictoryEventProbe.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace campaign_completion {

struct CompletionCandidate final {
    std::uint64_t sessionId = 0u;
    CompletionRecord record;
};

class CompletionCandidateCoordinator final {
public:
    using Clock = std::function<std::string()>;

    explicit CompletionCandidateCoordinator(Clock clock);
    void BeginSession(std::uint64_t sessionId,
                      LaunchOriginSnapshot origin) noexcept;
    std::optional<CompletionCandidate> ObserveIdentity(
        const ConfirmedMapIdentity& identity,
        LaunchOriginSnapshot refinedOrigin) noexcept;
    std::optional<CompletionCandidate> ObserveVictory(
        const VictoryEventSnapshot& victory) noexcept;
    void Disable() noexcept;

private:
    std::optional<CompletionCandidate> TryComplete() noexcept;

    Clock clock_;
    std::optional<ConfirmedMapIdentity> identity_;
    std::optional<VictoryEventSnapshot> victory_;
    LaunchOriginSnapshot origin_{};
    std::uint64_t sessionId_ = 0u;
    bool emitted_ = false;
    bool terminalInvalid_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
