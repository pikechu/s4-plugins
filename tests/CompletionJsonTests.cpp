#include "completion/CompletionJson.h"

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

campaign_completion::CompletionRecord FixedRecord(std::string suffix = "a") {
    using namespace campaign_completion;
    CompletionRecord record{};
    record.stableId = "map:map\\user\\" + suffix + ".map";
    record.relativeId = "Map\\User\\" + suffix + ".map";
    record.displayName = suffix;
    record.mapKind = CompletionMapKind::Fixed;
    record.launchSource = LaunchSource::SinglePlayerMap;
    record.completedAtUtc = "2026-07-14T01:57:38Z";
    return record;
}

campaign_completion::CompletionRecord RandomRecord() {
    using namespace campaign_completion;
    CompletionRecord record{};
    record.stableId = "map:rd_lcgsdr30";
    record.relativeId = "RD_LCGSDR30";
    record.displayName = "RD_LCGSDR30";
    record.mapKind = CompletionMapKind::Random;
    record.launchSource = LaunchSource::RandomMap;
    record.completedAtUtc = "2026-07-14T02:08:00Z";
    return record;
}

std::string ValidJson() {
    return R"json({"schema_version":1,"records":[{"stable_id":"map:map\\user\\a.map","relative_id":"Map\\User\\a.map","display_name":"a","map_kind":"fixed","launch_source":"single-player-map","completed_at_utc":"2026-07-14T01:57:38Z","record_source":"native-event-609","game_version":"2.50.1516.0","plugin_version":"0.4.0"}]})json";
}

std::string ReplaceOnce(std::string value,
                        const std::string& from,
                        const std::string& to) {
    const auto position = value.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture replacement target missing");
    }
    value.replace(position, from.size(), to);
    return value;
}

void RequireFailure(std::string bytes,
                    campaign_completion::CompletionJsonFailure failure,
                    const char* message) {
    const auto result = campaign_completion::DecodeCompletionJson(bytes);
    Require(!result && result.failure == failure, message);
}

}  // namespace

int RunCompletionJsonTests() {
    using namespace campaign_completion;

    CompletionDatabaseSnapshot unsorted{};
    unsorted.records.push_back(RandomRecord());
    unsorted.records.push_back(FixedRecord());
    const auto encoded = EncodeCompletionJson(unsorted);
    Require(encoded.has_value() && !encoded->empty() &&
                encoded->back() == '\n',
            "encoded JSON must end with one newline");
    const auto decoded = DecodeCompletionJson(*encoded);
    Require(decoded && decoded.snapshot.records.size() == 2u,
            "valid JSON must round-trip");
    Require(decoded.snapshot.records[0].stableId <
                decoded.snapshot.records[1].stableId,
            "records must be sorted by stable ID");
    Require(decoded.snapshot.records[0].relativeId ==
                "Map\\User\\a.map" &&
                decoded.snapshot.records[1].mapKind ==
                    CompletionMapKind::Random,
            "round-trip must preserve fixed and random fields");
    Require(EncodeCompletionJson(decoded.snapshot) == encoded,
            "encoding a decoded snapshot must be byte deterministic");

    const auto compact = DecodeCompletionJson(ValidJson());
    Require(compact && compact.snapshot.records.size() == 1u,
            "decoder must accept schema-valid compact JSON");

    RequireFailure(std::string(kMaximumCompletionJsonBytes + 1u, ' '),
                   CompletionJsonFailure::Oversized,
                   "input above 4 MiB must fail as oversized");

    auto illegalUtf8 = ValidJson();
    illegalUtf8 = ReplaceOnce(illegalUtf8, "\"display_name\":\"a\"",
                              "\"display_name\":\"x\"");
    illegalUtf8[illegalUtf8.find("\"display_name\":\"x\"") + 16u] =
        static_cast<char>(0xFFu);
    RequireFailure(illegalUtf8, CompletionJsonFailure::Utf8,
                   "illegal raw UTF-8 must be rejected");

    RequireFailure(
        ReplaceOnce(ValidJson(), "\"display_name\":\"a\"",
                    "\"display_name\":\"\\q\""),
        CompletionJsonFailure::Syntax,
        "unknown JSON escape must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"display_name\":\"a\"",
                    "\"display_name\":\"\\uD800\""),
        CompletionJsonFailure::Utf8,
        "lone JSON high surrogate must be rejected");

    RequireFailure(
        ReplaceOnce(ValidJson(), "\"schema_version\":1,",
                    "\"schema_version\":1,\"schema_version\":1,"),
        CompletionJsonFailure::DuplicateKey,
        "duplicate top-level key must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"display_name\":\"a\",",
                    "\"display_name\":\"a\",\"display_name\":\"a\","),
        CompletionJsonFailure::DuplicateKey,
        "duplicate record key must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"records\":[",
                    "\"extra\":0,\"records\":["),
        CompletionJsonFailure::UnknownField,
        "unknown top-level field must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"plugin_version\":\"0.4.0\"",
                    "\"plugin_version\":\"0.4.0\",\"extra\":\"x\""),
        CompletionJsonFailure::UnknownField,
        "unknown record field must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"display_name\":\"a\",", ""),
        CompletionJsonFailure::MissingField,
        "missing required field must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"map_kind\":\"fixed\"",
                    "\"map_kind\":1"),
        CompletionJsonFailure::WrongType,
        "wrong required field type must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"schema_version\":1",
                    "\"schema_version\":2"),
        CompletionJsonFailure::Schema,
        "unknown schema version must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"map_kind\":\"fixed\"",
                    "\"map_kind\":\"mystery\""),
        CompletionJsonFailure::InvalidRecord,
        "unknown map kind must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(),
                    "\"launch_source\":\"single-player-map\"",
                    "\"launch_source\":\"online-multiplayer\""),
        CompletionJsonFailure::InvalidRecord,
        "online source must not be loaded as a completion");
    RequireFailure(
        ReplaceOnce(ValidJson(), "2026-07-14T01:57:38Z",
                    "2026-02-29T01:57:38Z"),
        CompletionJsonFailure::InvalidRecord,
        "invalid UTC date must be rejected");
    RequireFailure(
        ReplaceOnce(ValidJson(), "map:map\\\\user\\\\a.map",
                    "map:wrong"),
        CompletionJsonFailure::InvalidRecord,
        "stable ID inconsistent with relative identity must be rejected");
    RequireFailure(ValidJson() + "x", CompletionJsonFailure::Syntax,
                   "trailing non-whitespace must be rejected");

    const auto longDisplay = std::string(1025u, 'a');
    RequireFailure(
        ReplaceOnce(ValidJson(), "\"display_name\":\"a\"",
                    "\"display_name\":\"" + longDisplay + "\""),
        CompletionJsonFailure::FieldLimit,
        "decoded string above 1024 units must be rejected");

    const auto valid = ValidJson();
    const auto arrayStart = valid.find('[');
    const auto arrayEnd = valid.rfind(']');
    const auto object = valid.substr(arrayStart + 1u,
                                     arrayEnd - arrayStart - 1u);
    const auto duplicateRecords =
        valid.substr(0u, arrayStart + 1u) + object + "," + object +
        valid.substr(arrayEnd);
    RequireFailure(duplicateRecords,
                   CompletionJsonFailure::DuplicateStableId,
                   "duplicate stable IDs must be rejected");

    CompletionDatabaseSnapshot duplicateSnapshot{};
    duplicateSnapshot.records = {FixedRecord(), FixedRecord()};
    Require(!EncodeCompletionJson(duplicateSnapshot),
            "encoder must reject duplicate stable IDs");

    CompletionDatabaseSnapshot tooMany{};
    tooMany.records.reserve(kMaximumCompletionRecords + 1u);
    for (std::size_t index = 0u;
         index <= kMaximumCompletionRecords; ++index) {
        tooMany.records.push_back(FixedRecord(std::to_string(index)));
    }
    Require(!EncodeCompletionJson(std::move(tooMany)),
            "encoder must reject more than 8192 records");

    auto invalidForEncode = FixedRecord();
    invalidForEncode.displayName.assign(1025u, 'a');
    CompletionDatabaseSnapshot invalidSnapshot{};
    invalidSnapshot.records.push_back(std::move(invalidForEncode));
    Require(!EncodeCompletionJson(std::move(invalidSnapshot)),
            "encoder must enforce decoded string bounds");
    return 0;
}
