#pragma once

#include <filesystem>

namespace campaign_completion {

bool ConsumeStopRequest(const std::filesystem::path& path);

}  // namespace campaign_completion
