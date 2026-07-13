#pragma once

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace campaign_completion {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

std::string FormatLogLine(LogLevel level,
                          std::string_view message,
                          const SYSTEMTIME& localTime);

class Logger final {
public:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool Open(const std::filesystem::path& path);
    void Write(LogLevel level, std::string_view message);
    void Close();

private:
    std::mutex mutex_;
    std::ofstream stream_;
};

}  // namespace campaign_completion
