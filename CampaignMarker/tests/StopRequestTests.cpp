#include "runtime/StopRequest.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunStopRequestTests() {
    const auto root = std::filesystem::temp_directory_path() /
                      (L"CampaignCompletionStopTests-" +
                       std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(GetTickCount64()));
    const auto request = root / L"CampaignCompletionDebug.stop";
    std::error_code cleanupError;
    try {
        std::filesystem::create_directories(root);
        Require(!campaign_completion::ConsumeStopRequest(request),
                "absent stop request must not be consumed");
        {
            std::ofstream output(request);
            output << "stop\n";
        }
        Require(campaign_completion::ConsumeStopRequest(request),
                "existing stop request must be consumed");
        Require(!std::filesystem::exists(request),
                "consumed stop request must be removed");
        Require(!campaign_completion::ConsumeStopRequest(request),
                "stop request must be one-shot");
    } catch (...) {
        std::filesystem::remove_all(root, cleanupError);
        throw;
    }
    std::filesystem::remove_all(root, cleanupError);
    return 0;
}
