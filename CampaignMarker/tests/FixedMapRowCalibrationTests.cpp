#include "marker/FixedMapRowCalibration.h"

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::CompletionRecord Record(
    std::string stableId, std::string relativeId, std::string displayName,
    campaign_completion::LaunchSource source) {
    campaign_completion::CompletionRecord record{};
    record.stableId = std::move(stableId);
    record.relativeId = std::move(relativeId);
    record.displayName = std::move(displayName);
    record.mapKind = campaign_completion::CompletionMapKind::Fixed;
    record.launchSource = source;
    record.completedAtUtc = "2026-07-14T03:00:00Z";
    return record;
}

campaign_completion::CompletionDatabaseSnapshot CompletedMaps() {
    using namespace campaign_completion;
    CompletionDatabaseSnapshot snapshot{};
    snapshot.records.push_back(Record(
        "map:map\\singleplayer\\aeneas.map",
        "Map\\Singleplayer\\Aeneas.map", "Aeneas",
        LaunchSource::SinglePlayerMap));
    snapshot.records.push_back(Record(
        "map:map\\user\\antares.map", "Map\\User\\Antares.map",
        "Antares", LaunchSource::SinglePlayerCustomMap));
    return snapshot;
}

campaign_completion::PageSnapshot FixedPages() {
    return {{4u, 22u, 23u, 25u}, 25u};
}

campaign_completion::MarkerGuiElement Element(std::string text) {
    campaign_completion::MarkerGuiElement element{};
    element.surfaceWidth = 800u;
    element.surfaceHeight = 600u;
    element.containerType = 1u;
    element.x = 100u;
    element.y = 120u;
    element.width = 300u;
    element.height = 24u;
    element.valueLink = 2422u;
    element.textStyle = 9u;
    element.text = std::move(text);
    return element;
}

std::size_t CountPrefix(const std::vector<std::string>& records,
                        std::string_view prefix) {
    std::size_t count = 0u;
    for (const auto& record : records) {
        if (record.rfind(prefix, 0u) == 0u) {
            ++count;
        }
    }
    return count;
}

const std::string& LastWithPrefix(const std::vector<std::string>& records,
                                  std::string_view prefix) {
    for (auto current = records.rbegin(); current != records.rend(); ++current) {
        if (current->rfind(prefix, 0u) == 0u) {
            return *current;
        }
    }
    throw std::runtime_error("expected trace prefix was absent");
}

}  // namespace

int RunFixedMapRowCalibrationTests() {
    using namespace campaign_completion;

    std::string copied;
    Require(!CopyBoundedGuiText(nullptr, copied),
            "null GUI text is rejected");
    Require(CopyBoundedGuiText("Aeneas", copied) && copied == "Aeneas",
            "bounded NUL-terminated GUI text is copied");
    const char withControl[] = {'A', '\x01', 'B', '\0'};
    Require(CopyBoundedGuiText(withControl, copied) && copied.size() == 3u,
            "copy operation preserves bytes for later validation");
    Require(!DecodeMenuTextLossless(copied),
            "decoded control character is rejected");
    std::array<char, 257u> unterminated{};
    unterminated.fill('a');
    Require(!CopyBoundedGuiText(unterminated.data(), copied),
            "GUI text without NUL inside 256 bytes is rejected");
    Require(DecodeMenuTextLossless("Aeneas") ==
                std::optional<std::wstring>(L"Aeneas"),
            "ASCII menu text round-trips through the active code page");

    CompletionMarkerIndex index;
    index.Publish(CompletedMaps());
    std::vector<std::string> records;
    FixedMapRowCalibration calibration(
        index, [&records](std::string_view record) {
            records.emplace_back(record);
            return true;
        });

    calibration.ObserveElement(Element("Aeneas"));
    Require(records.empty(), "candidate without page evidence is ignored");

    calibration.ObservePages({{4u, 22u, 23u}, 23u});
    calibration.ObserveListKind(FixedMapListKind::Single);
    records.clear();
    calibration.ObserveElement(Element("Aeneas"));
    Require(records.empty(), "non-exact page set is ignored");

    calibration.ObservePages(FixedPages());
    calibration.ObserveListKind(FixedMapListKind::Single);
    records.clear();
    calibration.ObserveElement(Element("Aeneas"));
    Require(CountPrefix(records, "marker-candidate=") == 1u,
            "single list records its completed candidate");
    const auto& unique = LastWithPrefix(records, "marker-candidate=");
    for (const auto* token : {
             "sequence-", "generation-", "list-single", "name-Aeneas",
             "rect-100,120,300,24", "surface-800,600",
             "value-link-2422", "container-1", "text-style-9",
             "match-unique"}) {
        Require(unique.find(token) != std::string::npos,
                "unique candidate record is missing a bounded field");
    }

    records.clear();
    calibration.ObserveElement(Element("Antares"));
    Require(records.empty(), "single list cannot record custom candidate");
    calibration.ObserveListKind(FixedMapListKind::Custom);
    records.clear();
    calibration.ObserveElement(Element("Aeneas"));
    calibration.ObserveElement(Element("Antares"));
    Require(CountPrefix(records, "marker-candidate=") == 1u &&
                LastWithPrefix(records, "marker-candidate=")
                        .find("list-custom") != std::string::npos,
            "custom list records only Antares");

    calibration.ObserveListKind(FixedMapListKind::Unknown);
    records.clear();
    calibration.ObserveElement(Element("Antares"));
    Require(records.empty(), "unknown list state is inert");
    calibration.ObservePages({{7u}, 7u});
    calibration.ObserveListKind(FixedMapListKind::Custom);
    records.clear();
    calibration.ObserveElement(Element("Antares"));
    Require(records.empty(), "leaving fixed pages resets list state");

    calibration.ObservePages(FixedPages());
    calibration.ObserveListKind(FixedMapListKind::Single);
    records.clear();
    auto invalid = Element("Aeneas");
    invalid.x = 700u;
    invalid.width = 200u;
    calibration.ObserveElement(invalid);
    auto zero = Element("Aeneas");
    zero.height = 0u;
    calibration.ObserveElement(zero);
    calibration.ObserveElement(Element(std::string(withControl, 3u)));
    Require(records.empty(),
            "invalid geometry and invalid decoded text are rejected");

    records.clear();
    calibration.ObserveElement(Element("Aeneas"));
    Require(CountPrefix(records, "marker-candidate=") == 1u,
            "valid candidate precedes one frame boundary");
    calibration.ObserveFrame(25u, 0);
    Require(CountPrefix(records, "marker-frame=") == 1u,
            "next frame closes the pending candidate sequence range");
    const auto& frame = LastWithPrefix(records, "marker-frame=");
    Require(frame.find("page-25") != std::string::npos &&
                frame.find("pillarbox-0") != std::string::npos &&
                frame.find("candidate-first-") != std::string::npos &&
                frame.find("candidate-last-") != std::string::npos,
            "frame boundary records callback-order evidence");
    calibration.ObserveFrame(25u, 0);
    Require(CountPrefix(records, "marker-frame=") == 1u,
            "frame without candidates emits no repeated boundary");

    auto duplicate = Record(
        "map:map\\singleplayer\\sub\\aeneas.map",
        "Map\\Singleplayer\\Sub\\Aeneas.map", "Aeneas",
        LaunchSource::SinglePlayerMap);
    auto ambiguousSnapshot = CompletedMaps();
    ambiguousSnapshot.records.push_back(std::move(duplicate));
    CompletionMarkerIndex ambiguousIndex;
    ambiguousIndex.Publish(ambiguousSnapshot);
    std::vector<std::string> ambiguousRecords;
    FixedMapRowCalibration ambiguous(
        ambiguousIndex, [&ambiguousRecords](std::string_view record) {
            ambiguousRecords.emplace_back(record);
            return true;
        });
    ambiguous.ObservePages(FixedPages());
    ambiguous.ObserveListKind(FixedMapListKind::Single);
    ambiguousRecords.clear();
    ambiguous.ObserveElement(Element("Aeneas"));
    Require(CountPrefix(ambiguousRecords, "marker-candidate=") == 1u &&
                LastWithPrefix(ambiguousRecords, "marker-candidate=")
                        .find("match-ambiguous") != std::string::npos,
            "duplicate candidates are explicitly ambiguous");

    ambiguous.Disable();
    ambiguousRecords.clear();
    ambiguous.ObservePages(FixedPages());
    ambiguous.ObserveListKind(FixedMapListKind::Single);
    ambiguous.ObserveElement(Element("Aeneas"));
    ambiguous.ObserveFrame(25u, 0);
    Require(ambiguousRecords.empty(), "disabled calibration remains inert");

    return 0;
}
