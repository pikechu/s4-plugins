#include "diagnostics/Logger.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

}  // namespace

int RunLoggerTests() {
    SYSTEMTIME time{};
    time.wYear = 2026;
    time.wMonth = 7;
    time.wDay = 13;
    time.wHour = 11;
    time.wMinute = 30;
    time.wSecond = 5;
    time.wMilliseconds = 42;

    const auto line = campaign_completion::FormatLogLine(
        campaign_completion::LogLevel::Info, "loaded", time);
    Require(line == "2026-07-13T11:30:05.042+LOCAL [INFO] loaded\n",
            "FormatLogLine must produce deterministic UTF-8 output");

    const auto root = std::filesystem::temp_directory_path() /
                      L"CampaignCompletionLoggerTests";
    std::filesystem::remove_all(root);
    const auto logPath = root / L"nested" / L"CampaignCompletion.log";

    campaign_completion::Logger logger;
    Require(logger.Open(logPath), "Logger::Open must create parent directories");
    logger.Write(campaign_completion::LogLevel::Info, "first");
    logger.Write(campaign_completion::LogLevel::Error, "second");
    logger.Close();

    const auto contents = ReadAll(logPath);
    Require(contents.find("[INFO] first\n") != std::string::npos,
            "first record must be newline terminated");
    Require(contents.find("[ERROR] second\n") != std::string::npos,
            "second record must be newline terminated");
    Require(std::filesystem::exists(logPath), "log file must exist");
    std::filesystem::remove_all(root);
    return 0;
}
