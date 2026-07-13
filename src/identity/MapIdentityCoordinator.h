#pragma once

#include "identity/ListAttribution.h"
#include "identity/LuaMapSession.h"

#include <cstdint>
#include <functional>
#include <string>

namespace campaign_completion {

class MapIdentityCoordinator {
public:
    using Sink = std::function<void(std::string)>;

    MapIdentityCoordinator(Sink recordSink, Sink traceSink);

    void ObserveListKind(FixedMapListKind kind, std::uint64_t nowMs) noexcept;
    void ObserveBack() noexcept;
    void ObserveLuaOpen(std::uint64_t nowMs) noexcept;
    std::uint64_t ObserveMapInit(std::uint64_t nowMs) noexcept;
    void ObserveTick(bool inGame,
                     std::uint64_t nowMs,
                     ILuaMapBridge& bridge) noexcept;
    void Disable() noexcept;
    std::uint64_t CurrentSessionId() const noexcept {
        return currentSessionId_;
    }

private:
    void Emit(std::string line) noexcept;
    void EmitResult(const LuaMapSessionResult& result) noexcept;

    Sink recordSink_;
    Sink traceSink_;
    LuaMapSession session_;
    FixedMapListKind currentList_ = FixedMapListKind::Unknown;
    FixedMapListKind sessionList_ = FixedMapListKind::Unknown;
    std::uint64_t luaGeneration_ = 0u;
    std::uint64_t currentSessionId_ = 0u;
    bool pending_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
