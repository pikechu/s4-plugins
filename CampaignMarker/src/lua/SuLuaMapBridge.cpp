#include "lua/SuLuaMapBridge.h"

#include <cstddef>

namespace campaign_completion {
namespace {

constexpr long kMaximumLuaValueBytes = 512;
char kSuTableName[] = "SU";
char kGameTableName[] = "Game";
char kMapNameFunction[] = "GetMapName";
char kRelativeMapNameFunction[] = "GetMapNameRelativePath";

class LuaBlock final {
public:
    explicit LuaBlock(ILuaApi& api) noexcept : api_(api) { api_.BeginBlock(); }
    ~LuaBlock() { api_.EndBlock(); }

    LuaBlock(const LuaBlock&) = delete;
    LuaBlock& operator=(const LuaBlock&) = delete;

private:
    ILuaApi& api_;
};

}  // namespace

LuaMapReadAttempt S4LuaMapBridge::Read() noexcept {
    LuaMapReadAttempt attempt;
    attempt.name = ReadValue(kMapNameFunction);
    attempt.relative = ReadValue(kRelativeMapNameFunction);
    return attempt;
}

LuaValueResult S4LuaMapBridge::ReadValue(char* functionName) noexcept {
    LuaValueResult result;
    try {
        LuaBlock block(api_);
        const auto su = api_.GetGlobal(kSuTableName);
        if (!api_.IsTable(su)) {
            result.status = SuMapReadStatus::SuTableMissing;
            return result;
        }
        const auto game = api_.GetField(su, kGameTableName);
        if (!api_.IsTable(game)) {
            result.status = SuMapReadStatus::GameTableMissing;
            return result;
        }
        const auto function = api_.GetField(game, functionName);
        if (!api_.IsFunction(function)) {
            result.status = SuMapReadStatus::FunctionMissing;
            return result;
        }
        if (!api_.CallFunction(function)) {
            result.status = SuMapReadStatus::CallError;
            return result;
        }
        const auto value = api_.GetResult(1);
        if (!api_.IsString(value)) {
            result.status = SuMapReadStatus::NonString;
            return result;
        }
        const char* bytes = api_.GetString(value);
        const long length = api_.StringLength(value);
        if (bytes == nullptr || length < 0) {
            result.status = SuMapReadStatus::NonString;
            return result;
        }
        if (length > kMaximumLuaValueBytes) {
            result.status = SuMapReadStatus::TooLong;
            return result;
        }
        result.bytes.assign(bytes, static_cast<std::size_t>(length));
        result.status = SuMapReadStatus::Success;
    } catch (...) {
        result.status = SuMapReadStatus::CallError;
        result.bytes.clear();
    }
    return result;
}

}  // namespace campaign_completion
