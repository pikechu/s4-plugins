#pragma once

#include "lua/SuLuaMapBridge.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace campaign_completion {

inline constexpr std::uint64_t kLuaRetryIntervalMs = 50u;
inline constexpr std::uint64_t kLuaSessionDeadlineMs = 5000u;
inline constexpr std::size_t kLuaMaximumAttempts = 64u;

struct LuaMapSessionResult {
    std::uint64_t sessionId = 0u;
    std::uint64_t luaGeneration = 0u;
    bool terminal = false;
    LuaValueResult name;
    LuaValueResult relative;
};

class LuaMapSession {
public:
    void ObserveLuaOpen(std::uint64_t nowMs) noexcept;
    std::uint64_t ObserveMapInit(std::uint64_t nowMs) noexcept;
    std::optional<LuaMapSessionResult> ObserveTick(
        bool inGame,
        std::uint64_t nowMs,
        ILuaMapBridge& bridge) noexcept;
    void Disable() noexcept;

private:
    static bool IsRetryable(SuMapReadStatus status) noexcept;
    static void ApplyAttemptValue(const LuaValueResult& observed,
                                  LuaValueResult& cached,
                                  bool& hasCached,
                                  SuMapReadStatus& terminalStatus) noexcept;
    std::optional<LuaMapSessionResult> Finish(
        SuMapReadStatus unresolvedStatus) noexcept;

    bool enabled_ = true;
    bool active_ = false;
    bool bound_ = false;
    bool stale_ = false;
    std::uint64_t luaGeneration_ = 0u;
    std::uint64_t boundGeneration_ = 0u;
    std::uint64_t nextSessionId_ = 0u;
    std::uint64_t sessionId_ = 0u;
    std::uint64_t startedMs_ = 0u;
    std::uint64_t nextAttemptMs_ = 0u;
    std::size_t attempts_ = 0u;
    bool hasName_ = false;
    bool hasRelative_ = false;
    LuaValueResult name_;
    LuaValueResult relative_;
};

}  // namespace campaign_completion
