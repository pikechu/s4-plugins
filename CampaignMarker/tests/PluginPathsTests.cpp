#include "runtime/PluginPaths.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunPluginPathsTests() {
    using campaign_completion::BuildPluginPaths;

    const auto paths = BuildPluginPaths(
        LR"(F:\Games\thesettlers4\Plugins\CampaignCompletionDebug.asi)");
    Require(paths.module ==
                LR"(F:\Games\thesettlers4\Plugins\CampaignCompletionDebug.asi)",
            "module path must remain normalized");
    Require(paths.pluginDirectory == LR"(F:\Games\thesettlers4\Plugins)",
            "plugin directory must be the ASI parent");
    Require(paths.dataDirectory ==
                LR"(F:\Games\thesettlers4\Plugins\CampaignCompletion)",
            "data directory must be below Plugins");
    Require(paths.ini ==
                paths.dataDirectory / L"CampaignCompletionDebug.ini",
            "INI must be module-relative");
    Require(paths.log == paths.dataDirectory / L"CampaignCompletion.log",
            "log must be module-relative");
    Require(paths.stop ==
                paths.dataDirectory / L"CampaignCompletionDebug.stop",
            "stop request must be module-relative");
    Require(paths.database == paths.dataDirectory / L"completed_maps.json",
            "database must be module-relative");
    Require(paths.temporary ==
                paths.dataDirectory / L"completed_maps.json.tmp",
            "temporary database must be module-relative");
    Require(paths.backup ==
                paths.dataDirectory / L"completed_maps.json.bak",
            "backup database must be module-relative");

    const auto normalized = BuildPluginPaths(
        LR"(F:\Games\thesettlers4\Plugins\nested\..\CampaignCompletionDebug.asi)");
    Require(normalized.module == paths.module,
            "lexical parent components must be normalized without probing");
    return 0;
}
