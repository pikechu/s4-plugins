#pragma once

namespace campaign_completion {

class ILuaApi {
public:
    using LuaObject = unsigned int;

    virtual ~ILuaApi() = default;
    virtual void BeginBlock() noexcept = 0;
    virtual void EndBlock() noexcept = 0;
    virtual LuaObject GetGlobal(char* name) noexcept = 0;
    virtual LuaObject GetField(LuaObject object, char* field) noexcept = 0;
    virtual bool IsTable(LuaObject object) noexcept = 0;
    virtual bool IsFunction(LuaObject object) noexcept = 0;
    virtual bool CallFunction(LuaObject function) noexcept = 0;
    virtual LuaObject GetResult(int index) noexcept = 0;
    virtual bool IsString(LuaObject object) noexcept = 0;
    virtual const char* GetString(LuaObject object) noexcept = 0;
    virtual long StringLength(LuaObject object) noexcept = 0;
};

class S4LuaApi final : public ILuaApi {
public:
    void BeginBlock() noexcept override;
    void EndBlock() noexcept override;
    LuaObject GetGlobal(char* name) noexcept override;
    LuaObject GetField(LuaObject object, char* field) noexcept override;
    bool IsTable(LuaObject object) noexcept override;
    bool IsFunction(LuaObject object) noexcept override;
    bool CallFunction(LuaObject function) noexcept override;
    LuaObject GetResult(int index) noexcept override;
    bool IsString(LuaObject object) noexcept override;
    const char* GetString(LuaObject object) noexcept override;
    long StringLength(LuaObject object) noexcept override;
};

}  // namespace campaign_completion
