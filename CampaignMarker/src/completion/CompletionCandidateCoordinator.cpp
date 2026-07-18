#include "completion/CompletionCandidateCoordinator.h"

#include "victory/MapSessionPolicy.h"

#include <utility>

namespace campaign_completion {
namespace {

std::wstring_view FixedDisplayName(std::wstring_view relative,
                                   std::wstring_view fallback) noexcept {
    if (relative.size() <= 4u ||
        CompareStringOrdinal(relative.data() + relative.size() - 4u, 4,
                             L".map", 4, TRUE) != CSTR_EQUAL) {
        return fallback;
    }
    const auto separator = relative.find_last_of(L"\\/");
    const std::size_t first =
        separator == std::wstring_view::npos ? 0u : separator + 1u;
    if (first >= relative.size() - 4u) {
        return fallback;
    }
    return relative.substr(first, relative.size() - first - 4u);
}

}  // namespace

CompletionCandidateCoordinator::CompletionCandidateCoordinator(Clock clock)
    : clock_(std::move(clock)) {}

void CompletionCandidateCoordinator::BeginSession(
    std::uint64_t sessionId,
    LaunchOriginSnapshot origin) noexcept {
    if (!enabled_) {
        return;
    }
    identity_.reset();
    victory_.reset();
    origin_ = origin;
    sessionId_ = sessionId;
    emitted_ = false;
    terminalInvalid_ = false;
}

std::optional<CompletionCandidate>
CompletionCandidateCoordinator::ObserveIdentity(
    const ConfirmedMapIdentity& identity,
    LaunchOriginSnapshot refinedOrigin) noexcept {
    if (!enabled_ || sessionId_ == 0u ||
        identity.sessionId != sessionId_) {
        return std::nullopt;
    }
    try {
        identity_ = identity;
        origin_ = refinedOrigin;
        return TryComplete();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<CompletionCandidate>
CompletionCandidateCoordinator::ObserveVictory(
    const VictoryEventSnapshot& victory) noexcept {
    if (!enabled_ || sessionId_ == 0u ||
        victory.sessionId != sessionId_) {
        return std::nullopt;
    }
    try {
        if (victory.result != NativeLocalResult::Won ||
            victory.fields.eventId != kNativeTerminalEventId) {
            terminalInvalid_ = true;
            victory_.reset();
            return std::nullopt;
        }
        victory_ = victory;
        return TryComplete();
    } catch (...) {
        return std::nullopt;
    }
}

void CompletionCandidateCoordinator::Disable() noexcept {
    identity_.reset();
    victory_.reset();
    origin_ = {};
    sessionId_ = 0u;
    emitted_ = false;
    terminalInvalid_ = true;
    enabled_ = false;
}

std::optional<CompletionCandidate>
CompletionCandidateCoordinator::TryComplete() noexcept {
    if (!enabled_ || emitted_ || terminalInvalid_ || sessionId_ == 0u ||
        !identity_.has_value() || !victory_.has_value() ||
        identity_->sessionId != sessionId_ ||
        victory_->sessionId != sessionId_ ||
        victory_->result != NativeLocalResult::Won ||
        !ShouldRecordVictory(origin_) || !clock_) {
        return std::nullopt;
    }
    try {
        const auto stableId = BuildStableMapId(identity_->relative);
        const auto relativeId = WideToStrictUtf8(identity_->relative);
        const auto mapKind = CompletionKindFor(identity_->relative);
        const auto displayName = WideToStrictUtf8(
            mapKind == CompletionMapKind::Fixed
                ? FixedDisplayName(identity_->relative, identity_->name)
                : std::wstring_view(identity_->name));
        const std::string completedAtUtc = clock_();
        if (!stableId.has_value() || !relativeId.has_value() ||
            !displayName.has_value() ||
            !IsValidUtcCompletionTime(completedAtUtc)) {
            return std::nullopt;
        }

        CompletionCandidate candidate{};
        candidate.sessionId = sessionId_;
        candidate.record.stableId = *stableId;
        candidate.record.relativeId = *relativeId;
        candidate.record.displayName = *displayName;
        candidate.record.mapKind = mapKind;
        candidate.record.launchSource = origin_.source;
        candidate.record.completedAtUtc = completedAtUtc;
        emitted_ = true;
        return candidate;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace campaign_completion
