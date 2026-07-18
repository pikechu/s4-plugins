#include "manager/FixedMapDiscovery.h"

#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int RunFixedMapDiscoveryTests() {
    using namespace campaign_completion;
    const std::vector<FixedMapDirectoryEntry> valid{
        {L"Aeneas.map", FILE_ATTRIBUTE_NORMAL},
        {L"notes.txt", FILE_ATTRIBUTE_NORMAL},
        {L"Phoenix.MAP", FILE_ATTRIBUTE_ARCHIVE},
    };
    const auto accepted = ValidateFixedMapDirectory(
        L"Map\\Singleplayer",
        CompletionManagerCategory::FixedSinglePlayer, valid);
    Require(accepted.status == FixedMapDiscoveryStatus::Success &&
                accepted.maps.size() == 2u &&
                accepted.maps[0u].relativeId ==
                    L"Map\\Singleplayer\\Aeneas.map" &&
                accepted.maps[1u].displayName == L"Phoenix",
            "only exact nonrecursive map files are admitted");

    const auto wrongRoot = ValidateFixedMapDirectory(
        L"Map\\Singleplayer\\nested",
        CompletionManagerCategory::FixedSinglePlayer, valid);
    Require(wrongRoot.status == FixedMapDiscoveryStatus::Invalid,
            "non-exact roots fail closed");

    const auto reparse = ValidateFixedMapDirectory(
        L"Map\\User", CompletionManagerCategory::FixedCustom,
        {{L"Antares.map", FILE_ATTRIBUTE_REPARSE_POINT}});
    Require(reparse.status == FixedMapDiscoveryStatus::Invalid &&
                reparse.maps.empty(),
            "reparse points fail closed");

    const auto directory = ValidateFixedMapDirectory(
        L"Map\\User", CompletionManagerCategory::FixedCustom,
        {{L"nested", FILE_ATTRIBUTE_DIRECTORY}});
    Require(directory.status == FixedMapDiscoveryStatus::Invalid,
            "nested directories fail closed");

    const auto traversal = ValidateFixedMapDirectory(
        L"Map\\User", CompletionManagerCategory::FixedCustom,
        {{L"..\\Antares.map", FILE_ATTRIBUTE_NORMAL}});
    Require(traversal.status == FixedMapDiscoveryStatus::Success &&
                traversal.maps.empty(),
            "traversal-like names are never admitted");
    return 0;
}
