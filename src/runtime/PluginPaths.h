#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>

namespace campaign_completion {

struct PluginPaths final {
    std::filesystem::path module;
    std::filesystem::path pluginDirectory;
    std::filesystem::path dataDirectory;
    std::filesystem::path ini;
    std::filesystem::path log;
    std::filesystem::path stop;
    std::filesystem::path database;
    std::filesystem::path temporary;
    std::filesystem::path backup;
};

PluginPaths BuildPluginPaths(const std::filesystem::path& modulePath);
std::optional<PluginPaths> ResolvePluginPaths(HMODULE module) noexcept;

}  // namespace campaign_completion
