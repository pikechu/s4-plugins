#include "identity/MapIdentityCoordinator.h"

#include "identity/SuMapValue.h"

#include <string_view>
#include <utility>

namespace campaign_completion {
namespace {

bool IsUnavailable(SuMapReadStatus status) noexcept {
    return status == SuMapReadStatus::SuTableMissing ||
           status == SuMapReadStatus::GameTableMissing ||
           status == SuMapReadStatus::FunctionMissing ||
           status == SuMapReadStatus::Timeout;
}

}  // namespace

MapIdentityCoordinator::MapIdentityCoordinator(Sink recordSink,
                                               Sink traceSink)
    : recordSink_(std::move(recordSink)), traceSink_(std::move(traceSink)) {}

void MapIdentityCoordinator::ObserveListKind(FixedMapListKind kind,
                                             std::uint64_t) noexcept {
    if (enabled_) {
        currentList_ = kind;
    }
}

void MapIdentityCoordinator::ObserveBack() noexcept {
    if (!enabled_) {
        return;
    }
    currentList_ = FixedMapListKind::Unknown;
    sessionList_ = FixedMapListKind::Unknown;
    currentSessionId_ = 0u;
    pending_ = false;
}

void MapIdentityCoordinator::ObserveLuaOpen(std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return;
    }
    ++luaGeneration_;
    session_.ObserveLuaOpen(nowMs);
    Emit("lua-open-generation=" + std::to_string(luaGeneration_));
}

std::uint64_t MapIdentityCoordinator::ObserveMapInit(
    std::uint64_t nowMs) noexcept {
    if (!enabled_) {
        return 0u;
    }

    sessionList_ = currentList_;
    currentSessionId_ = session_.ObserveMapInit(nowMs);
    pending_ = currentSessionId_ != 0u;
    Emit("map-init-session=" + std::to_string(currentSessionId_) + ";list=" +
         std::string(FixedMapListKindName(sessionList_)));
    return currentSessionId_;
}

void MapIdentityCoordinator::ObserveTick(bool inGame,
                                         std::uint64_t nowMs,
                                         ILuaMapBridge& bridge) noexcept {
    if (!enabled_ || !pending_) {
        return;
    }

    const auto result = session_.ObserveTick(inGame, nowMs, bridge);
    if (!result.has_value()) {
        return;
    }
    pending_ = false;
    EmitResult(*result);
}

void MapIdentityCoordinator::Disable() noexcept {
    enabled_ = false;
    pending_ = false;
    currentList_ = FixedMapListKind::Unknown;
    sessionList_ = FixedMapListKind::Unknown;
    currentSessionId_ = 0u;
    session_.Disable();
}

void MapIdentityCoordinator::Emit(std::string line) noexcept {
    try {
        if (recordSink_) {
            recordSink_(line);
        }
        if (traceSink_) {
            traceSink_(std::move(line));
        }
    } catch (...) {
    }
}

void MapIdentityCoordinator::EmitResult(
    const LuaMapSessionResult& result) noexcept {
    try {
        ValidatedSuValue name;
        if (result.name.status == SuMapReadStatus::Success) {
            name = ValidateSuMapName(result.name.bytes);
        } else {
            name.status = result.name.status;
        }

        ValidatedSuValue relative;
        if (result.relative.status == SuMapReadStatus::Success) {
            relative = ValidateSuRelativePath(result.relative.bytes);
        } else {
            relative.status = result.relative.status;
        }

        Emit("su-map-name-status=" +
             std::string(SuMapReadStatusName(name.status)));
        Emit("su-map-relative-status=" +
             std::string(SuMapReadStatusName(relative.status)));
        if (name) {
            Emit("su-map-name=" + name.displayUtf8);
        }
        if (relative) {
            Emit("su-map-relative=" + relative.displayUtf8);
        }

        std::string_view association = "invalid-value";
        if (name.status == SuMapReadStatus::StaleGeneration ||
            relative.status == SuMapReadStatus::StaleGeneration) {
            association = "stale-generation";
        } else if (name.status == SuMapReadStatus::ValueConflict ||
                   relative.status == SuMapReadStatus::ValueConflict) {
            association = "conflict";
        } else if (name && relative) {
            association = "confirmed";
        } else if (name && IsUnavailable(relative.status)) {
            association = "name-only";
        } else if (IsUnavailable(name.status) &&
                   IsUnavailable(relative.status)) {
            association = "lua-unavailable";
        }
        Emit("identity-association=" + std::string(association));
    } catch (...) {
        Emit("su-map-name-status=encoding-failure");
        Emit("su-map-relative-status=encoding-failure");
        Emit("identity-association=invalid-value");
    }
}

}  // namespace campaign_completion
