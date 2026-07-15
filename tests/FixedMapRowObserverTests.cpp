#include "marker/FixedMapRowObserver.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

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
        "map:map\\singleplayer\\atlas.map",
        "Map\\Singleplayer\\Atlas.map", "Atlas",
        LaunchSource::SinglePlayerMap));
    snapshot.records.push_back(Record(
        "map:map\\user\\antares.map", "Map\\User\\Antares.map",
        "Antares", LaunchSource::SinglePlayerCustomMap));
    return snapshot;
}

campaign_completion::PageSnapshot FixedPages() {
    return {{4u, 22u, 23u, 25u}, 25u};
}

campaign_completion::FixedMapRowObservation Element(
    const char* label, std::size_t slot = 0u) {
    using namespace campaign_completion;
    FixedMapRowObservation element{};
    element.surfaceWidth = 800u;
    element.surfaceHeight = 600u;
    element.containerType = 0u;
    element.x = 115u;
    element.y = static_cast<WORD>(142u + 31u * slot);
    element.width = 271u;
    element.height = 30u;
    element.valueLink = static_cast<WORD>(2417u + slot);
    element.textStyle = 1u;
    if (!CopyBoundedGuiText(label, element.text)) {
        throw std::runtime_error("observer test label did not fit");
    }
    return element;
}

bool IsCommand(const campaign_completion::MarkerDrawCommand& command,
               std::size_t slot) {
    return command.logicalSurfaceWidth == 800u &&
           command.logicalSurfaceHeight == 600u && command.x == 115u &&
           command.y == static_cast<WORD>(142u + 31u * slot) &&
           command.width == 271u && command.height == 30u;
}

void Admit(campaign_completion::FixedMapRowObserver& observer,
           campaign_completion::FixedMapListKind kind) {
    observer.ObservePages(FixedPages());
    observer.ObserveListKind(kind);
}

}  // namespace

int RunFixedMapRowObserverTests() {
    using namespace campaign_completion;

    CompletionMarkerIndex index;
    index.Publish(CompletedMaps());

    {
        FixedMapRowObserver observer(index);
        observer.ObserveElement(Element("Aeneas"));
        Require(observer.TakeFrame(25u).count == 0u,
                "row evidence without exact pages and a tab is inert");

        observer.ObservePages({{4u, 22u, 23u}, 23u});
        observer.ObserveListKind(FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        Require(observer.TakeFrame(25u).count == 0u,
                "a non-exact fixed page set fails closed");

        Admit(observer, FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        const auto wrongPage = observer.TakeFrame(4u);
        Require(wrongPage.count == 0u,
                "an earlier page-4 frame never draws or consumes the row");
        const auto frame = observer.TakeFrame(25u);
        Require(frame.count == 1u && IsCommand(frame.commands[0], 0u),
                "the final page-25 layer consumes the calibrated Aeneas row");
        Require(observer.TakeFrame(25u).count == 1u,
                "a validated visible row redraws on every later frame");
        observer.ObserveElement(Element("AllGreen"));
        Require(observer.TakeFrame(25u).count == 0u,
                "a newly observed incomplete row clears only its slot");
    }

    {
        FixedMapRowObserver observer(index);
        observer.ObservePages(FixedPages());
        observer.ObserveElement(Element("Aeneas"));
        Require(observer.TakeFrame(25u).count == 1u,
                "first entry defaults safely to the visible single-map tab");
    }

    {
        FixedMapRowObserver observer(index);
        Admit(observer, FixedMapListKind::Custom);
        observer.ObserveElement(Element("Antares", 2u));
        const auto custom = observer.TakeFrame(25u);
        Require(custom.count == 1u && IsCommand(custom.commands[0], 2u),
                "custom category accepts only its completed Antares row");

        observer.ObserveListKind(FixedMapListKind::Multiplayer);
        observer.ObserveElement(Element("Antares"));
        Require(observer.TakeFrame(25u).count == 0u,
                "multiplayer category does not borrow a custom completion");
    }

    {
        FixedMapRowObserver observer(index);
        Admit(observer, FixedMapListKind::Single);
        auto padded = Element("  Aeneas  ");
        padded.textStyle = 2u;
        observer.ObserveElement(padded);
        Require(observer.TakeFrame(25u).count == 1u,
                "trimmed label and both calibrated text styles are accepted");

        const auto valid = Element("Aeneas");
        std::array<FixedMapRowObservation, 9u> rejected{};
        rejected.fill(valid);
        rejected[0].surfaceWidth = 1024u;
        rejected[1].surfaceHeight = 768u;
        rejected[2].containerType = 1u;
        rejected[3].x = 114u;
        rejected[4].width = 270u;
        rejected[5].height = 29u;
        rejected[6].y = 143u;
        rejected[7].valueLink = 2418u;
        rejected[8].textStyle = 3u;
        for (const auto& element : rejected) {
            FixedMapRowObserver rejectedObserver(index);
            Admit(rejectedObserver, FixedMapListKind::Single);
            rejectedObserver.ObserveElement(element);
            Require(rejectedObserver.TakeFrame(25u).count == 0u,
                    "every mismatch in the calibrated signature fails closed");
        }
    }

    {
        FixedMapRowObserver observer(index);
        Admit(observer, FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        observer.ObserveElement(Element("Aeneas"));
        Require(observer.TakeFrame(25u).count == 1u,
                "identical repeated callbacks collapse to one command");

        observer.ObserveElement(Element("Aeneas", 0u));
        observer.ObserveElement(Element("Aeneas", 1u));
        Require(observer.TakeFrame(25u).count == 0u,
                "one label at two valid rectangles is ambiguous for the frame");

        observer.ObserveElement(Element("Aeneas", 0u));
        observer.ObserveElement(Element("Atlas", 0u));
        observer.ObserveElement(Element("Aeneas", 1u));
        Require(observer.TakeFrame(25u).count == 0u,
                "two completed labels in one slot invalidate the whole frame");
    }

    {
        CompletionDatabaseSnapshot sevenMaps{};
        for (std::size_t slot = 0u; slot <= kMaximumVisibleFixedRows; ++slot) {
            const auto name = "Row" + std::to_string(slot);
            sevenMaps.records.push_back(Record(
                "map:map\\singleplayer\\row" + std::to_string(slot) +
                    ".map",
                "Map\\Singleplayer\\" + name + ".map", name,
                LaunchSource::SinglePlayerMap));
        }
        CompletionMarkerIndex sevenIndex;
        sevenIndex.Publish(sevenMaps);
        FixedMapRowObserver observer(sevenIndex);
        Admit(observer, FixedMapListKind::Single);
        for (std::size_t slot = 0u; slot < kMaximumVisibleFixedRows; ++slot) {
            const auto name = "Row" + std::to_string(slot);
            observer.ObserveElement(Element(name.c_str(), slot));
        }
        const auto full = observer.TakeFrame(25u);
        Require(full.count == kMaximumVisibleFixedRows,
                "the fixed frame capacity admits exactly six calibrated rows");

        FixedMapRowObserver overflow(sevenIndex);
        Admit(overflow, FixedMapListKind::Single);
        for (std::size_t slot = 0u; slot <= kMaximumVisibleFixedRows; ++slot) {
            const auto name = "Row" + std::to_string(slot);
            overflow.ObserveElement(Element(name.c_str(), slot));
        }
        Require(overflow.TakeFrame(25u).count == 0u,
                "exhausting the six-row guard invalidates the whole frame");
    }

    {
        FixedMapRowObserver observer(index);
        Admit(observer, FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        observer.ObserveListKind(FixedMapListKind::Custom);
        Require(observer.TakeFrame(25u).count == 0u,
                "a tab change clears pending rows");

        observer.ObserveListKind(FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        observer.ObservePages({{7u}, 7u});
        Require(observer.TakeFrame(25u).count == 0u,
                "a page change clears pending rows");

        Admit(observer, FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        observer.Disable();
        observer.ObservePages(FixedPages());
        observer.ObserveListKind(FixedMapListKind::Single);
        observer.ObserveElement(Element("Aeneas"));
        Require(observer.TakeFrame(25u).count == 0u,
                "Disable clears state and remains inert");
    }

    return 0;
}
