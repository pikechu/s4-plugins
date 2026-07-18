#include "marker/CompletionMarkerIndex.h"

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
    campaign_completion::CompletionMapKind mapKind,
    campaign_completion::LaunchSource launchSource) {
    campaign_completion::CompletionRecord record{};
    record.stableId = std::move(stableId);
    record.relativeId = std::move(relativeId);
    record.displayName = std::move(displayName);
    record.mapKind = mapKind;
    record.launchSource = launchSource;
    record.completedAtUtc = "2026-07-14T03:00:00Z";
    return record;
}

campaign_completion::CompletionRecord Aeneas() {
    using namespace campaign_completion;
    return Record("map:map\\singleplayer\\aeneas.map",
                  "Map\\Singleplayer\\Aeneas.map", "Aeneas",
                  CompletionMapKind::Fixed, LaunchSource::SinglePlayerMap);
}

campaign_completion::CompletionRecord Alien() {
    using namespace campaign_completion;
    return Record("map:map\\multiplayer\\alien.map",
                  "map/multiplayer/Alien.map", "Alien",
                  CompletionMapKind::Fixed,
                  LaunchSource::SinglePlayerMultiplayerMap);
}

campaign_completion::CompletionRecord Antares() {
    using namespace campaign_completion;
    return Record("map:map\\user\\antares.map",
                  "MAP\\USER\\Antares.map", "Antares",
                  CompletionMapKind::Fixed,
                  LaunchSource::SinglePlayerCustomMap);
}

}  // namespace

int RunCompletionMarkerIndexTests() {
    using namespace campaign_completion;

    CompletionDatabaseSnapshot snapshot{};
    snapshot.records = {Aeneas(), Alien(), Antares()};
    CompletionMarkerIndex index;
    index.Publish(snapshot);

    const auto single = index.Find(FixedMapListKind::Single, L"aEnEaS");
    Require(single.size() == 1u &&
                single.front().stableId ==
                    "map:map\\singleplayer\\aeneas.map" &&
                single.front().listKind == FixedMapListKind::Single,
            "single-player candidate is found only by ordinal name match");
    Require(index.Find(FixedMapListKind::Multiplayer, L"Alien").size() == 1u,
            "offline multiplayer candidate is indexed");
    Require(index.Find(FixedMapListKind::Custom, L"ANTARES").size() == 1u,
            "custom candidate is indexed");
    Require(index.Find(FixedMapListKind::Custom, L"Aeneas").empty() &&
                index.Find(FixedMapListKind::Unknown, L"Aeneas").empty(),
            "list category and unknown state fail closed");
    Require(index.Match(FixedMapListKind::Single, L"Aeneas") ==
                MarkerMatchStatus::Unique,
            "one category-correct record must match uniquely");
    Require(index.Match(FixedMapListKind::Custom, L"Aeneas") ==
                MarkerMatchStatus::None,
            "the same name in a wrong category must not match");
    Require(index.Match(FixedMapListKind::Unknown, L"Aeneas") ==
                MarkerMatchStatus::None,
            "unknown category must fail closed");
    Require(index.MatchRelative(
                FixedMapListKind::Single,
                L"map/singleplayer/AENEAS.map") ==
                MarkerMatchStatus::Unique,
            "menu relative identifiers match across case and slash variants");
    Require(index.MatchRelative(FixedMapListKind::Custom,
                                L"Map\\Singleplayer\\Aeneas.map") ==
                MarkerMatchStatus::None &&
                index.MatchRelative(FixedMapListKind::Single, L"Aeneas") ==
                    MarkerMatchStatus::None,
            "relative matching rejects a wrong list and a display label");

    auto random = Antares();
    random.stableId = "map:random";
    random.mapKind = CompletionMapKind::Random;
    auto online = Antares();
    online.stableId = "map:online";
    online.launchSource = LaunchSource::OnlineMultiplayer;
    auto loadedOnline = Antares();
    loadedOnline.stableId = "map:loaded-online";
    loadedOnline.launchSource = LaunchSource::LoadOnlineMultiplayer;
    auto campaign = Aeneas();
    campaign.stableId = "map:campaign";
    campaign.launchSource = LaunchSource::Campaign;
    auto badUtf8 = Antares();
    badUtf8.stableId = "map:bad-utf8";
    badUtf8.displayName = std::string("\xC0\xAF", 2u);
    auto empty = Antares();
    empty.stableId = "map:empty";
    empty.displayName.clear();
    auto oversized = Antares();
    oversized.stableId = "map:oversized";
    oversized.displayName.assign(257u, 'a');
    auto mismatch = Antares();
    mismatch.stableId = "map:mismatch";
    mismatch.displayName = "Different";
    auto prefixConfusion = Antares();
    prefixConfusion.stableId = "map:prefix-confusion";
    prefixConfusion.relativeId = "Map\\Userland\\Antares.map";

    CompletionDatabaseSnapshot rejected{};
    rejected.records = {random,       online,   loadedOnline, campaign,
                        badUtf8,      empty,    oversized,    mismatch,
                        prefixConfusion};
    index.Publish(rejected);
    Require(index.Find(FixedMapListKind::Single, L"Aeneas").empty() &&
                index.Find(FixedMapListKind::Custom, L"Antares").empty(),
            "ineligible and malformed records never enter the index");
    Require(index.MatchRelative(FixedMapListKind::Custom,
                                L"Map\\User\\Antares.map") ==
                MarkerMatchStatus::None,
            "random and other ineligible records never match by relative id");

    auto duplicate = Antares();
    duplicate.stableId = "map:map\\user\\sub\\antares.map";
    duplicate.relativeId = "Map\\User\\Sub\\Antares.map";
    CompletionDatabaseSnapshot ambiguous{};
    ambiguous.records = {Antares(), duplicate};
    index.Publish(ambiguous);
    Require(index.Find(FixedMapListKind::Custom, L"Antares").size() == 2u,
            "duplicate display candidates remain visible to fail-closed caller");
    Require(index.Match(FixedMapListKind::Custom, L"Antares") ==
                MarkerMatchStatus::Ambiguous,
            "two indexed candidates with one display name must be ambiguous");

    auto sameRelative = Antares();
    sameRelative.stableId = "map:duplicate-relative";
    ambiguous.records = {Antares(), sameRelative};
    index.Publish(ambiguous);
    Require(index.MatchRelative(FixedMapListKind::Custom,
                                L"map/user/ANTARES.map") ==
                MarkerMatchStatus::Ambiguous,
            "duplicate relative candidates fail closed as ambiguous");

    CompletionDatabaseSnapshot replacement{};
    replacement.records = {Aeneas()};
    index.Publish(replacement);
    Require(index.Find(FixedMapListKind::Custom, L"Antares").empty() &&
                index.Find(FixedMapListKind::Single, L"Aeneas").size() == 1u,
            "publishing a replacement removes stale candidates");

    return 0;
}
