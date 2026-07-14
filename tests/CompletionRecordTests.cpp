#include "completion/CompletionRecord.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunCompletionRecordTests() {
    using namespace campaign_completion;

    const auto canonical =
        BuildStableMapId(L"Map/User/Battle of the Gods.map");
    Require(canonical == std::optional<std::string>(
                             "map:map\\user\\battle of the gods.map"),
            "stable ID must normalize separator and case");
    Require(BuildStableMapId(L"MAP\\USER\\BATTLE OF THE GODS.MAP") ==
                canonical,
            "stable ID must be case-insensitive");
    Require(!BuildStableMapId(L""), "empty identity must be rejected");
    Require(!BuildStableMapId(L"C:\\absolute.map"),
            "drive-absolute identity must be rejected");
    Require(!BuildStableMapId(L"\\\\server\\share\\absolute.map"),
            "UNC identity must be rejected");
    Require(!BuildStableMapId(L"\\rooted.map"),
            "rooted identity must be rejected");
    Require(!BuildStableMapId(L"Map\\..\\escape.map"),
            "parent traversal must be rejected");
    Require(!BuildStableMapId(L"Map\\.\\alias.map"),
            "dot path components must be rejected");

    std::wstring embeddedNull = L"Map\\User\\A";
    embeddedNull.push_back(L'\0');
    embeddedNull += L".map";
    Require(!BuildStableMapId(embeddedNull),
            "embedded NUL must be rejected");
    Require(!BuildStableMapId(std::wstring(513u, L'a')),
            "identity above 512 UTF-16 units must be rejected");

    std::wstring loneHigh(1u, static_cast<wchar_t>(0xD800u));
    std::wstring loneLow(1u, static_cast<wchar_t>(0xDC00u));
    Require(!WideToStrictUtf8(loneHigh),
            "lone high surrogate must be rejected");
    Require(!WideToStrictUtf8(loneLow),
            "lone low surrogate must be rejected");
    Require(WideToStrictUtf8(L"\u6218\u5F79") ==
                std::optional<std::string>("\xE6\x88\x98\xE5\xBD\xB9"),
            "non-ASCII text must use strict UTF-8");
    Require(StrictUtf8ToWide("\xE6\x88\x98\xE5\xBD\xB9") ==
                std::optional<std::wstring>(L"\u6218\u5F79"),
            "strict UTF-8 conversion must preserve non-ASCII text");
    Require(!StrictUtf8ToWide("\xC0\xAF"),
            "overlong UTF-8 must be rejected");
    const std::string utf8EmbeddedNull("Map\0User", 8u);
    Require(!StrictUtf8ToWide(utf8EmbeddedNull),
            "UTF-8 with an embedded NUL must be rejected");

    Require(CompletionKindFor(L"RD_LCGSDR30") ==
                CompletionMapKind::Random,
            "RD_ identifier must be random");
    Require(CompletionKindFor(L"[seed]") == CompletionMapKind::Random,
            "square random identifier must be random");
    Require(CompletionKindFor(L"<seed>") == CompletionMapKind::Random,
            "angle random identifier must be random");
    Require(CompletionKindFor(L"rd_LCGSDR30") == CompletionMapKind::Fixed,
            "random prefix remains case-sensitive");
    Require(CompletionKindFor(L"Map\\User\\RD_Test.map") ==
                CompletionMapKind::Fixed,
            "ordinary path containing RD must remain fixed");

    SYSTEMTIME utc{2026, 7, 2, 14, 1, 57, 38, 0};
    Require(FormatUtcCompletionTime(utc) ==
                std::optional<std::string>("2026-07-14T01:57:38Z"),
            "UTC time must use whole-second ISO 8601");
    Require(IsValidUtcCompletionTime("2024-02-29T23:59:59Z"),
            "valid leap day must pass");
    Require(!IsValidUtcCompletionTime("2026-02-29T00:00:00Z"),
            "invalid calendar date must fail");
    Require(!IsValidUtcCompletionTime("2026-07-14T01:57:38.000Z"),
            "fractional seconds must fail schema version 1");
    Require(!IsValidUtcCompletionTime("2026-07-14T01:57:38+08:00"),
            "non-UTC offsets must fail schema version 1");
    const auto current = CurrentUtcCompletionTime();
    Require(IsValidUtcCompletionTime(current),
            "current Win32 UTC time must satisfy the same validator");

    CompletionRecord record{};
    Require(record.recordSource == "native-event-609",
            "record source must remain fixed for version 0.5.0");
    Require(record.gameVersion == "2.50.1516.0",
            "game version must be persisted exactly");
    Require(record.pluginVersion == "0.5.0",
            "plugin version must be persisted exactly");
    return 0;
}
