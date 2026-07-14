#include "completion/CompletionJson.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace campaign_completion {
namespace {

CompletionJsonFailure ValidateString(std::string_view value) noexcept {
    const auto wide = StrictUtf8ToWide(value);
    if (!wide.has_value()) {
        return CompletionJsonFailure::Utf8;
    }
    if (wide->size() > kMaximumCompletionStringUnits) {
        return CompletionJsonFailure::FieldLimit;
    }
    return CompletionJsonFailure::None;
}

bool IsAllowedLaunchSource(LaunchSource source) noexcept {
    switch (source) {
        case LaunchSource::SinglePlayerMap:
        case LaunchSource::SinglePlayerMultiplayerMap:
        case LaunchSource::SinglePlayerCustomMap:
        case LaunchSource::Campaign:
        case LaunchSource::RandomMap:
        case LaunchSource::LoadCampaign:
        case LaunchSource::LoadSinglePlayerMap:
            return true;
        case LaunchSource::Unknown:
        case LaunchSource::OnlineMultiplayer:
        case LaunchSource::LoadMapUnresolved:
        case LaunchSource::LoadOnlineMultiplayer:
            return false;
    }
    return false;
}

bool IsAllowedPluginVersion(std::string_view version) noexcept {
    return version == "0.4.0" || version == kCompletionPluginVersion;
}

std::optional<LaunchSource> ParseLaunchSource(
    std::string_view value) noexcept {
    for (const auto source : {
             LaunchSource::SinglePlayerMap,
             LaunchSource::SinglePlayerMultiplayerMap,
             LaunchSource::SinglePlayerCustomMap,
             LaunchSource::Campaign,
             LaunchSource::RandomMap,
             LaunchSource::LoadCampaign,
             LaunchSource::LoadSinglePlayerMap}) {
        if (LaunchSourceName(source) == value) {
            return source;
        }
    }
    return std::nullopt;
}

CompletionJsonFailure ValidateRecord(
    const CompletionRecord& record) noexcept {
    for (const auto value : {
             std::string_view(record.stableId),
             std::string_view(record.relativeId),
             std::string_view(record.displayName),
             std::string_view(record.completedAtUtc),
             std::string_view(record.recordSource),
             std::string_view(record.gameVersion),
             std::string_view(record.pluginVersion)}) {
        const auto failure = ValidateString(value);
        if (failure != CompletionJsonFailure::None) {
            return failure;
        }
    }
    if (record.recordSource != kCompletionRecordSource ||
        record.gameVersion != kCompletionGameVersion ||
        !IsAllowedPluginVersion(record.pluginVersion) ||
        !IsValidUtcCompletionTime(record.completedAtUtc) ||
        !IsAllowedLaunchSource(record.launchSource)) {
        return CompletionJsonFailure::InvalidRecord;
    }
    const auto relative = StrictUtf8ToWide(record.relativeId);
    if (!relative.has_value()) {
        return CompletionJsonFailure::Utf8;
    }
    const auto stableId = BuildStableMapId(*relative);
    if (!stableId.has_value() || *stableId != record.stableId ||
        CompletionKindFor(*relative) != record.mapKind) {
        return CompletionJsonFailure::InvalidRecord;
    }
    const bool randomSource = record.launchSource == LaunchSource::RandomMap;
    const bool randomKind = record.mapKind == CompletionMapKind::Random;
    if (randomSource != randomKind) {
        return CompletionJsonFailure::InvalidRecord;
    }
    return CompletionJsonFailure::None;
}

class JsonParser final {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    CompletionJsonResult Parse() noexcept {
        CompletionJsonResult result{};
        try {
            if (!ParseTop(result.snapshot)) {
                result.snapshot = {};
                result.failure = failure_;
                return result;
            }
            SkipWhitespace();
            if (position_ != input_.size()) {
                result.snapshot = {};
                result.failure = CompletionJsonFailure::Syntax;
                return result;
            }
            std::sort(result.snapshot.records.begin(),
                      result.snapshot.records.end(),
                      [](const auto& left, const auto& right) {
                          return left.stableId < right.stableId;
                      });
            for (std::size_t index = 1u;
                 index < result.snapshot.records.size(); ++index) {
                if (result.snapshot.records[index - 1u].stableId ==
                    result.snapshot.records[index].stableId) {
                    result.snapshot = {};
                    result.failure =
                        CompletionJsonFailure::DuplicateStableId;
                    return result;
                }
            }
            return result;
        } catch (...) {
            result.snapshot = {};
            result.failure = failure_ == CompletionJsonFailure::None
                                 ? CompletionJsonFailure::Syntax
                                 : failure_;
            return result;
        }
    }

private:
    bool Fail(CompletionJsonFailure failure) noexcept {
        if (failure_ == CompletionJsonFailure::None) {
            failure_ = failure;
        }
        return false;
    }

    void SkipWhitespace() noexcept {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' &&
                value != '\n') {
                break;
            }
            ++position_;
        }
    }

    char Peek() noexcept {
        SkipWhitespace();
        return position_ < input_.size() ? input_[position_] : '\0';
    }

    bool Expect(char expected) noexcept {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != expected) {
            return Fail(CompletionJsonFailure::Syntax);
        }
        ++position_;
        return true;
    }

    static int HexValue(char value) noexcept {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    }

    bool ParseHex4(std::uint16_t& output) noexcept {
        if (position_ + 4u > input_.size()) {
            return Fail(CompletionJsonFailure::Syntax);
        }
        unsigned value = 0u;
        for (std::size_t index = 0u; index < 4u; ++index) {
            const int digit = HexValue(input_[position_ + index]);
            if (digit < 0) {
                return Fail(CompletionJsonFailure::Syntax);
            }
            value = value * 16u + static_cast<unsigned>(digit);
        }
        position_ += 4u;
        output = static_cast<std::uint16_t>(value);
        return true;
    }

    bool AppendEscapedUnicode(std::string& output) noexcept {
        std::uint16_t first = 0u;
        if (!ParseHex4(first)) {
            return false;
        }
        std::wstring units;
        try {
            units.push_back(static_cast<wchar_t>(first));
            if (first >= 0xD800u && first <= 0xDBFFu) {
                if (position_ + 6u > input_.size() ||
                    input_[position_] != '\\' ||
                    input_[position_ + 1u] != 'u') {
                    return Fail(CompletionJsonFailure::Utf8);
                }
                position_ += 2u;
                std::uint16_t second = 0u;
                if (!ParseHex4(second)) {
                    return false;
                }
                if (second < 0xDC00u || second > 0xDFFFu) {
                    return Fail(CompletionJsonFailure::Utf8);
                }
                units.push_back(static_cast<wchar_t>(second));
            } else if (first >= 0xDC00u && first <= 0xDFFFu) {
                return Fail(CompletionJsonFailure::Utf8);
            }
            const auto encoded = WideToStrictUtf8(units);
            if (!encoded.has_value()) {
                return Fail(CompletionJsonFailure::Utf8);
            }
            output += *encoded;
            return true;
        } catch (...) {
            return Fail(CompletionJsonFailure::Syntax);
        }
    }

    bool ParseString(std::string& output) noexcept {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != '"') {
            return Fail(CompletionJsonFailure::WrongType);
        }
        ++position_;
        try {
            output.clear();
            while (position_ < input_.size()) {
                const unsigned char value =
                    static_cast<unsigned char>(input_[position_++]);
                if (value == static_cast<unsigned char>('"')) {
                    const auto wide = StrictUtf8ToWide(output);
                    if (!wide.has_value()) {
                        return Fail(CompletionJsonFailure::Utf8);
                    }
                    if (wide->size() > kMaximumCompletionStringUnits) {
                        return Fail(CompletionJsonFailure::FieldLimit);
                    }
                    return true;
                }
                if (value < 0x20u) {
                    return Fail(CompletionJsonFailure::Syntax);
                }
                if (value != static_cast<unsigned char>('\\')) {
                    output.push_back(static_cast<char>(value));
                    continue;
                }
                if (position_ >= input_.size()) {
                    return Fail(CompletionJsonFailure::Syntax);
                }
                const char escaped = input_[position_++];
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        output.push_back(escaped);
                        break;
                    case 'b':
                        output.push_back('\b');
                        break;
                    case 'f':
                        output.push_back('\f');
                        break;
                    case 'n':
                        output.push_back('\n');
                        break;
                    case 'r':
                        output.push_back('\r');
                        break;
                    case 't':
                        output.push_back('\t');
                        break;
                    case 'u':
                        if (!AppendEscapedUnicode(output)) {
                            return false;
                        }
                        break;
                    default:
                        return Fail(CompletionJsonFailure::Syntax);
                }
            }
            return Fail(CompletionJsonFailure::Syntax);
        } catch (...) {
            return Fail(CompletionJsonFailure::Syntax);
        }
    }

    bool ParseUnsigned(unsigned& output) noexcept {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] < '0' ||
            input_[position_] > '9') {
            return Fail(CompletionJsonFailure::WrongType);
        }
        if (input_[position_] == '0' && position_ + 1u < input_.size() &&
            input_[position_ + 1u] >= '0' &&
            input_[position_ + 1u] <= '9') {
            return Fail(CompletionJsonFailure::Syntax);
        }
        unsigned value = 0u;
        while (position_ < input_.size() && input_[position_] >= '0' &&
               input_[position_] <= '9') {
            const unsigned digit =
                static_cast<unsigned>(input_[position_] - '0');
            if (value > ((std::numeric_limits<unsigned>::max)() - digit) /
                            10u) {
                return Fail(CompletionJsonFailure::Schema);
            }
            value = value * 10u + digit;
            ++position_;
        }
        output = value;
        return true;
    }

    bool ParseTop(CompletionDatabaseSnapshot& snapshot) noexcept {
        if (!Expect('{')) {
            return false;
        }
        unsigned fields = 0u;
        unsigned schemaVersion = 0u;
        if (Peek() == '}') {
            return Fail(CompletionJsonFailure::MissingField);
        }
        while (true) {
            std::string key;
            if (!ParseString(key) || !Expect(':')) {
                return false;
            }
            unsigned bit = 0u;
            if (key == "schema_version") {
                bit = 1u;
                if ((fields & bit) != 0u) {
                    return Fail(CompletionJsonFailure::DuplicateKey);
                }
                if (!ParseUnsigned(schemaVersion)) {
                    return false;
                }
            } else if (key == "records") {
                bit = 2u;
                if ((fields & bit) != 0u) {
                    return Fail(CompletionJsonFailure::DuplicateKey);
                }
                if (!ParseRecords(snapshot)) {
                    return false;
                }
            } else {
                return Fail(CompletionJsonFailure::UnknownField);
            }
            fields |= bit;
            const char separator = Peek();
            if (separator == ',') {
                ++position_;
                continue;
            }
            if (separator == '}') {
                ++position_;
                break;
            }
            return Fail(CompletionJsonFailure::Syntax);
        }
        if (fields != 3u) {
            return Fail(CompletionJsonFailure::MissingField);
        }
        if (schemaVersion != 1u) {
            return Fail(CompletionJsonFailure::Schema);
        }
        return true;
    }

    bool ParseRecords(CompletionDatabaseSnapshot& snapshot) noexcept {
        if (!Expect('[')) {
            return false;
        }
        if (Peek() == ']') {
            ++position_;
            return true;
        }
        while (true) {
            CompletionRecord record{};
            if (!ParseRecord(record)) {
                return false;
            }
            try {
                snapshot.records.push_back(std::move(record));
            } catch (...) {
                return Fail(CompletionJsonFailure::Syntax);
            }
            if (snapshot.records.size() > kMaximumCompletionRecords) {
                return Fail(CompletionJsonFailure::FieldLimit);
            }
            const char separator = Peek();
            if (separator == ',') {
                ++position_;
                continue;
            }
            if (separator == ']') {
                ++position_;
                return true;
            }
            return Fail(CompletionJsonFailure::Syntax);
        }
    }

    bool ParseRecord(CompletionRecord& record) noexcept {
        if (!Expect('{')) {
            return false;
        }
        std::uint16_t fields = 0u;
        constexpr std::uint16_t kAllFields =
            static_cast<std::uint16_t>((1u << 9u) - 1u);
        if (Peek() == '}') {
            return Fail(CompletionJsonFailure::MissingField);
        }
        while (true) {
            std::string key;
            if (!ParseString(key) || !Expect(':')) {
                return false;
            }
            int field = -1;
            if (key == "stable_id") field = 0;
            else if (key == "relative_id") field = 1;
            else if (key == "display_name") field = 2;
            else if (key == "map_kind") field = 3;
            else if (key == "launch_source") field = 4;
            else if (key == "completed_at_utc") field = 5;
            else if (key == "record_source") field = 6;
            else if (key == "game_version") field = 7;
            else if (key == "plugin_version") field = 8;
            else return Fail(CompletionJsonFailure::UnknownField);

            const auto bit = static_cast<std::uint16_t>(1u << field);
            if ((fields & bit) != 0u) {
                return Fail(CompletionJsonFailure::DuplicateKey);
            }
            std::string value;
            if (!ParseString(value)) {
                return false;
            }
            switch (field) {
                case 0: record.stableId = std::move(value); break;
                case 1: record.relativeId = std::move(value); break;
                case 2: record.displayName = std::move(value); break;
                case 3:
                    if (value == "fixed") {
                        record.mapKind = CompletionMapKind::Fixed;
                    } else if (value == "random") {
                        record.mapKind = CompletionMapKind::Random;
                    } else {
                        return Fail(CompletionJsonFailure::InvalidRecord);
                    }
                    break;
                case 4: {
                    const auto source = ParseLaunchSource(value);
                    if (!source.has_value()) {
                        return Fail(CompletionJsonFailure::InvalidRecord);
                    }
                    record.launchSource = *source;
                    break;
                }
                case 5: record.completedAtUtc = std::move(value); break;
                case 6: record.recordSource = std::move(value); break;
                case 7: record.gameVersion = std::move(value); break;
                case 8: record.pluginVersion = std::move(value); break;
                default: return Fail(CompletionJsonFailure::UnknownField);
            }
            fields = static_cast<std::uint16_t>(fields | bit);
            const char separator = Peek();
            if (separator == ',') {
                ++position_;
                continue;
            }
            if (separator == '}') {
                ++position_;
                break;
            }
            return Fail(CompletionJsonFailure::Syntax);
        }
        if (fields != kAllFields) {
            return Fail(CompletionJsonFailure::MissingField);
        }
        const auto validation = ValidateRecord(record);
        return validation == CompletionJsonFailure::None
                   ? true
                   : Fail(validation);
    }

    std::string_view input_;
    std::size_t position_ = 0u;
    CompletionJsonFailure failure_ = CompletionJsonFailure::None;
};

bool AppendJsonString(std::string_view value, std::string& output) {
    static constexpr char kHex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char raw : value) {
        const auto byte = static_cast<unsigned char>(raw);
        switch (byte) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (byte < 0x20u) {
                    output += "\\u00";
                    output.push_back(kHex[(byte >> 4u) & 0x0Fu]);
                    output.push_back(kHex[byte & 0x0Fu]);
                } else {
                    output.push_back(static_cast<char>(byte));
                }
                break;
        }
    }
    output.push_back('"');
    return output.size() <= kMaximumCompletionJsonBytes;
}

bool AppendField(std::string& output,
                 std::string_view key,
                 std::string_view value,
                 bool first) {
    if (!first) {
        output.push_back(',');
    }
    if (!AppendJsonString(key, output)) {
        return false;
    }
    output.push_back(':');
    return AppendJsonString(value, output);
}

}  // namespace

std::optional<std::string> EncodeCompletionJson(
    CompletionDatabaseSnapshot snapshot) noexcept {
    if (snapshot.records.size() > kMaximumCompletionRecords) {
        return std::nullopt;
    }
    try {
        std::sort(snapshot.records.begin(), snapshot.records.end(),
                  [](const auto& left, const auto& right) {
                      return left.stableId < right.stableId;
                  });
        for (std::size_t index = 0u; index < snapshot.records.size(); ++index) {
            const auto validation = ValidateRecord(snapshot.records[index]);
            if (validation != CompletionJsonFailure::None ||
                (index != 0u &&
                 snapshot.records[index - 1u].stableId ==
                     snapshot.records[index].stableId)) {
                return std::nullopt;
            }
        }

        std::string output;
        output.reserve(4096u);
        output += "{\"schema_version\":1,\"records\":[";
        for (std::size_t index = 0u; index < snapshot.records.size(); ++index) {
            if (index != 0u) {
                output.push_back(',');
            }
            output.push_back('{');
            const auto& record = snapshot.records[index];
            if (!AppendField(output, "stable_id", record.stableId, true) ||
                !AppendField(output, "relative_id", record.relativeId, false) ||
                !AppendField(output, "display_name", record.displayName, false) ||
                !AppendField(output, "map_kind",
                             record.mapKind == CompletionMapKind::Random
                                 ? "random" : "fixed", false) ||
                !AppendField(output, "launch_source",
                             LaunchSourceName(record.launchSource), false) ||
                !AppendField(output, "completed_at_utc",
                             record.completedAtUtc, false) ||
                !AppendField(output, "record_source",
                             record.recordSource, false) ||
                !AppendField(output, "game_version",
                             record.gameVersion, false) ||
                !AppendField(output, "plugin_version",
                             record.pluginVersion, false)) {
                return std::nullopt;
            }
            output.push_back('}');
            if (output.size() > kMaximumCompletionJsonBytes) {
                return std::nullopt;
            }
        }
        output += "]}\n";
        if (output.size() > kMaximumCompletionJsonBytes) {
            return std::nullopt;
        }
        return output;
    } catch (...) {
        return std::nullopt;
    }
}

CompletionJsonResult DecodeCompletionJson(std::string_view bytes) noexcept {
    if (bytes.size() > kMaximumCompletionJsonBytes) {
        CompletionJsonResult result{};
        result.failure = CompletionJsonFailure::Oversized;
        return result;
    }
    return JsonParser(bytes).Parse();
}

}  // namespace campaign_completion
