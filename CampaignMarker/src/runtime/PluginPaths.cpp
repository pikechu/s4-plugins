#include "runtime/PluginPaths.h"

#include <string>

namespace campaign_completion {

PluginPaths BuildPluginPaths(const std::filesystem::path& modulePath) {
    PluginPaths paths{};
    paths.module = modulePath.lexically_normal();
    paths.pluginDirectory = paths.module.parent_path();
    paths.dataDirectory = paths.pluginDirectory / L"CampaignCompletion";
    paths.ini = paths.dataDirectory / L"CampaignCompletionDebug.ini";
    paths.log = paths.dataDirectory / L"CampaignCompletion.log";
    paths.stop = paths.dataDirectory / L"CampaignCompletionDebug.stop";
    paths.database = paths.dataDirectory / L"completed_maps.json";
    paths.temporary = paths.dataDirectory / L"completed_maps.json.tmp";
    paths.backup = paths.dataDirectory / L"completed_maps.json.bak";
    return paths;
}

std::optional<PluginPaths> ResolvePluginPaths(HMODULE module) noexcept {
    if (module == nullptr) {
        return std::nullopt;
    }
    try {
        std::wstring modulePath(32768u, L'\0');
        const DWORD length = GetModuleFileNameW(
            module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (length == 0u ||
            length == static_cast<DWORD>(modulePath.size())) {
            return std::nullopt;
        }
        modulePath.resize(length);
        return BuildPluginPaths(modulePath);
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace campaign_completion
