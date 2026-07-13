#include "lua/LuaApi.h"

#include "S4ModApi.h"

namespace campaign_completion {

void S4LuaApi::BeginBlock() noexcept { lua_beginblock(); }

void S4LuaApi::EndBlock() noexcept { lua_endblock(); }

ILuaApi::LuaObject S4LuaApi::GetGlobal(char* name) noexcept {
    return lua_getglobal(name);
}

ILuaApi::LuaObject S4LuaApi::GetField(LuaObject object, char* field) noexcept {
    lua_pushobject(object);
    lua_pushstring(field);
    return lua_gettable();
}

bool S4LuaApi::IsTable(LuaObject object) noexcept {
    return lua_istable(object) != 0;
}

bool S4LuaApi::IsFunction(LuaObject object) noexcept {
    return lua_isfunction(object) != 0;
}

bool S4LuaApi::CallFunction(LuaObject function) noexcept {
    return lua_callfunction(function) == 0;
}

ILuaApi::LuaObject S4LuaApi::GetResult(int index) noexcept {
    return lua_getresult(index);
}

bool S4LuaApi::IsString(LuaObject object) noexcept {
    return lua_isstring(object) != 0;
}

const char* S4LuaApi::GetString(LuaObject object) noexcept {
    return lua_getstring(object);
}

long S4LuaApi::StringLength(LuaObject object) noexcept {
    return lua_strlen(object);
}

}  // namespace campaign_completion
