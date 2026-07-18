#pragma once

#include "identity/SuMapValue.h"
#include "lua/LuaApi.h"

#include <string>

namespace campaign_completion {

struct LuaValueResult {
    SuMapReadStatus status = SuMapReadStatus::FunctionMissing;
    std::string bytes;
};

struct LuaMapReadAttempt {
    LuaValueResult name;
    LuaValueResult relative;
};

class ILuaMapBridge {
public:
    virtual ~ILuaMapBridge() = default;
    virtual LuaMapReadAttempt Read() noexcept = 0;
};

class S4LuaMapBridge final : public ILuaMapBridge {
public:
    explicit S4LuaMapBridge(ILuaApi& api) noexcept : api_(api) {}

    LuaMapReadAttempt Read() noexcept override;

private:
    LuaValueResult ReadValue(char* functionName) noexcept;

    ILuaApi& api_;
};

}  // namespace campaign_completion
