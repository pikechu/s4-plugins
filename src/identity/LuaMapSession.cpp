#include "identity/LuaMapSession.h"

#include <utility>

namespace campaign_completion {

namespace {

LuaValueResult StatusValue(SuMapReadStatus status) {
    LuaValueResult value;
    value.status = status;
    return value;
}

}  // namespace

void LuaMapSession::ObserveLuaOpen(std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return;
    }

    ++luaGeneration_;
    if (!active_) {
        return;
    }

    if (bound_) {
        if (attempts_ == 0u) {
            boundGeneration_ = luaGeneration_;
            stale_ = false;
            nextAttemptMs_ = nowMs;
        } else {
            stale_ = true;
        }
        return;
    }

    bound_ = true;
    boundGeneration_ = luaGeneration_;
    nextAttemptMs_ = nowMs;
}

std::uint64_t LuaMapSession::ObserveMapInit(std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return 0u;
    }

    active_ = true;
    bound_ = luaGeneration_ != 0u;
    stale_ = false;
    boundGeneration_ = bound_ ? luaGeneration_ : 0u;
    sessionId_ = ++nextSessionId_;
    startedMs_ = nowMs;
    nextAttemptMs_ = nowMs + kLuaRetryIntervalMs;
    attempts_ = 0u;
    hasName_ = false;
    hasRelative_ = false;
    name_ = LuaValueResult{};
    relative_ = LuaValueResult{};
    return sessionId_;
}

std::optional<LuaMapSessionResult> LuaMapSession::ObserveTick(
    bool inGame,
    std::uint64_t nowMs,
    ILuaMapBridge& bridge) noexcept {
    if (!enabled_ || !active_ || !inGame || !bound_) {
        return std::nullopt;
    }

    if (stale_ || boundGeneration_ != luaGeneration_) {
        return Finish(SuMapReadStatus::StaleGeneration);
    }

    if (nowMs - startedMs_ >= kLuaSessionDeadlineMs) {
        return Finish(SuMapReadStatus::Timeout);
    }

    if (nowMs < nextAttemptMs_) {
        return std::nullopt;
    }

    const LuaMapReadAttempt observed = bridge.Read();
    ++attempts_;

    SuMapReadStatus nameTerminal = SuMapReadStatus::Success;
    SuMapReadStatus relativeTerminal = SuMapReadStatus::Success;
    ApplyAttemptValue(observed.name, name_, hasName_, nameTerminal);
    ApplyAttemptValue(observed.relative, relative_, hasRelative_,
                      relativeTerminal);

    if (nameTerminal != SuMapReadStatus::Success) {
        if (!hasName_) {
            name_ = StatusValue(nameTerminal);
        }
        if (!hasRelative_) {
            relative_ = observed.relative;
        }
        return Finish(nameTerminal);
    }
    if (relativeTerminal != SuMapReadStatus::Success) {
        if (!hasRelative_) {
            relative_ = StatusValue(relativeTerminal);
        }
        if (!hasName_) {
            name_ = observed.name;
        }
        return Finish(relativeTerminal);
    }

    if (hasName_ && hasRelative_) {
        return Finish(SuMapReadStatus::Success);
    }

    if (attempts_ >= kLuaMaximumAttempts) {
        return Finish(SuMapReadStatus::Timeout);
    }

    nextAttemptMs_ = nowMs + kLuaRetryIntervalMs;
    return std::nullopt;
}

void LuaMapSession::Disable() noexcept {
    enabled_ = false;
    active_ = false;
    bound_ = false;
    stale_ = false;
    hasName_ = false;
    hasRelative_ = false;
}

bool LuaMapSession::IsRetryable(SuMapReadStatus status) noexcept {
    return status == SuMapReadStatus::SuTableMissing ||
           status == SuMapReadStatus::GameTableMissing ||
           status == SuMapReadStatus::FunctionMissing;
}

void LuaMapSession::ApplyAttemptValue(const LuaValueResult& observed,
                                      LuaValueResult& cached,
                                      bool& hasCached,
                                      SuMapReadStatus& terminalStatus) noexcept {
    if (observed.status == SuMapReadStatus::Success) {
        if (hasCached && cached.bytes != observed.bytes) {
            cached = StatusValue(SuMapReadStatus::ValueConflict);
            hasCached = false;
            terminalStatus = SuMapReadStatus::ValueConflict;
            return;
        }
        if (!hasCached) {
            cached = observed;
            hasCached = true;
        }
        return;
    }

    if (!IsRetryable(observed.status)) {
        cached = observed;
        hasCached = false;
        terminalStatus = observed.status;
    } else if (!hasCached) {
        cached = observed;
    }
}

std::optional<LuaMapSessionResult> LuaMapSession::Finish(
    SuMapReadStatus unresolvedStatus) noexcept {
    LuaMapSessionResult result;
    result.sessionId = sessionId_;
    result.luaGeneration = boundGeneration_;
    result.terminal = true;

    if (hasName_) {
        result.name = name_;
    } else if (name_.status == SuMapReadStatus::ValueConflict ||
               (!IsRetryable(name_.status) &&
                name_.status != SuMapReadStatus::Success)) {
        result.name = name_;
    } else {
        result.name = StatusValue(unresolvedStatus);
    }

    if (hasRelative_) {
        result.relative = relative_;
    } else if (relative_.status == SuMapReadStatus::ValueConflict ||
               (!IsRetryable(relative_.status) &&
                relative_.status != SuMapReadStatus::Success)) {
        result.relative = relative_;
    } else {
        result.relative = StatusValue(unresolvedStatus);
    }

    active_ = false;
    return result;
}

}  // namespace campaign_completion
