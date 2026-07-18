#include "lua/LuaApi.h"
#include "lua/SuLuaMapBridge.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeLuaApi final : public campaign_completion::ILuaApi {
public:
    void BeginBlock() noexcept override { ++blocksBegun; }

    void EndBlock() noexcept override {
        ++blocksEnded;
        if (mutateOnEnd) {
            if (currentFunction == kNameFunction) {
                name = "changed-after-block";
            } else if (currentFunction == kRelativeFunction) {
                relative = "changed-after-block";
            }
        }
    }

    LuaObject GetGlobal(char* value) noexcept override {
        lookups.emplace_back(value == nullptr ? "<null>" : value);
        return suTablePresent ? kSuTable : 0u;
    }

    LuaObject GetField(LuaObject object, char* field) noexcept override {
        const std::string value = field == nullptr ? "<null>" : field;
        lookups.push_back(value);
        if (object == kSuTable && value == "Game") {
            return gameTablePresent ? kGameTable : 0u;
        }
        if (object == kGameTable && value == "GetMapName") {
            return nameFunctionPresent ? kNameFunction : 0u;
        }
        if (object == kGameTable && value == "GetMapNameRelativePath") {
            return relativeFunctionPresent ? kRelativeFunction : 0u;
        }
        return 0u;
    }

    bool IsTable(LuaObject object) noexcept override {
        return object == kSuTable || object == kGameTable;
    }

    bool IsFunction(LuaObject object) noexcept override {
        return object == kNameFunction || object == kRelativeFunction;
    }

    bool CallFunction(LuaObject function) noexcept override {
        currentFunction = function;
        calls.push_back(function);
        return callSucceeds;
    }

    LuaObject GetResult(int index) noexcept override {
        if (index != 1) {
            return 0u;
        }
        return currentFunction == kNameFunction ? kNameResult : kRelativeResult;
    }

    bool IsString(LuaObject object) noexcept override {
        return stringResult &&
               (object == kNameResult || object == kRelativeResult);
    }

    const char* GetString(LuaObject object) noexcept override {
        if (nullString) {
            return nullptr;
        }
        return object == kNameResult ? name.data() : relative.data();
    }

    long StringLength(LuaObject object) noexcept override {
        if (forcedLength >= 0) {
            return forcedLength;
        }
        return static_cast<long>(object == kNameResult ? name.size()
                                                       : relative.size());
    }

    static constexpr LuaObject kSuTable = 1u;
    static constexpr LuaObject kGameTable = 2u;
    static constexpr LuaObject kNameFunction = 3u;
    static constexpr LuaObject kRelativeFunction = 4u;
    static constexpr LuaObject kNameResult = 5u;
    static constexpr LuaObject kRelativeResult = 6u;

    bool suTablePresent = true;
    bool gameTablePresent = true;
    bool nameFunctionPresent = true;
    bool relativeFunctionPresent = true;
    bool callSucceeds = true;
    bool stringResult = true;
    bool nullString = false;
    bool mutateOnEnd = false;
    long forcedLength = -1;
    std::string name = "Aeneas";
    std::string relative = "Maps\\Single\\Aeneas.map";
    LuaObject currentFunction = 0u;
    int blocksBegun = 0;
    int blocksEnded = 0;
    std::vector<LuaObject> calls;
    std::vector<std::string> lookups;
};

}  // namespace

int RunSuLuaMapBridgeTests() {
    using campaign_completion::S4LuaMapBridge;
    using campaign_completion::SuMapReadStatus;

    FakeLuaApi successApi;
    successApi.mutateOnEnd = true;
    S4LuaMapBridge successBridge(successApi);
    const auto success = successBridge.Read();
    Require(success.name.status == SuMapReadStatus::Success &&
                success.name.bytes == "Aeneas",
            "map name is copied before block end");
    Require(success.relative.status == SuMapReadStatus::Success &&
                success.relative.bytes == "Maps\\Single\\Aeneas.map",
            "relative map path is copied before block end");
    Require(successApi.blocksBegun == 2 && successApi.blocksEnded == 2,
            "each function read has one balanced Lua block");
    Require(successApi.calls ==
                std::vector<FakeLuaApi::LuaObject>(
                    {FakeLuaApi::kNameFunction,
                     FakeLuaApi::kRelativeFunction}),
            "both zero-argument functions are called in order");
    Require(successApi.lookups ==
                std::vector<std::string>(
                    {"SU", "Game", "GetMapName", "SU", "Game",
                     "GetMapNameRelativePath"}),
            "each read follows the exact SU Game lookup chain");

    const auto requireBalancedFailure = [](FakeLuaApi api,
                                           SuMapReadStatus expected,
                                           const char* message) {
        S4LuaMapBridge bridge(api);
        const auto result = bridge.Read();
        Require(result.name.status == expected, message);
        Require(api.blocksBegun == 2 && api.blocksEnded == 2,
                "failure paths balance every Lua block");
    };

    FakeLuaApi missingSu;
    missingSu.suTablePresent = false;
    requireBalancedFailure(missingSu, SuMapReadStatus::SuTableMissing,
                           "missing SU table is explicit");

    FakeLuaApi missingGame;
    missingGame.gameTablePresent = false;
    requireBalancedFailure(missingGame, SuMapReadStatus::GameTableMissing,
                           "missing Game table is explicit");

    FakeLuaApi missingFunction;
    missingFunction.nameFunctionPresent = false;
    requireBalancedFailure(missingFunction, SuMapReadStatus::FunctionMissing,
                           "missing function is explicit");

    FakeLuaApi callError;
    callError.callSucceeds = false;
    requireBalancedFailure(callError, SuMapReadStatus::CallError,
                           "Lua call error is explicit");

    FakeLuaApi nonString;
    nonString.stringResult = false;
    requireBalancedFailure(nonString, SuMapReadStatus::NonString,
                           "non-string return is explicit");

    FakeLuaApi nullString;
    nullString.nullString = true;
    requireBalancedFailure(nullString, SuMapReadStatus::NonString,
                           "null Lua string is explicit");

    FakeLuaApi tooLong;
    tooLong.forcedLength = 513;
    requireBalancedFailure(tooLong, SuMapReadStatus::TooLong,
                           "over-limit Lua string is rejected before copy");
    return 0;
}
