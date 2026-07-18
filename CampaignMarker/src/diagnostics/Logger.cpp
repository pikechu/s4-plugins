#include "diagnostics/Logger.h"

#include <iomanip>
#include <sstream>
#include <system_error>

namespace campaign_completion {
namespace {

const char* LevelName(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace

std::string FormatLogLine(LogLevel level,
                          std::string_view message,
                          const SYSTEMTIME& localTime) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << localTime.wYear << '-'
           << std::setw(2) << localTime.wMonth << '-' << std::setw(2)
           << localTime.wDay << 'T' << std::setw(2) << localTime.wHour << ':'
           << std::setw(2) << localTime.wMinute << ':' << std::setw(2)
           << localTime.wSecond << '.' << std::setw(3) << localTime.wMilliseconds
           << "+LOCAL [" << LevelName(level) << "] " << message << '\n';
    return output.str();
}

bool Logger::Open(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    stream_.open(path, std::ios::binary | std::ios::out | std::ios::app);
    return stream_.is_open();
}

void Logger::Write(LogLevel level, std::string_view message) {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    const auto line = FormatLogLine(level, message, localTime);

    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }
    stream_.write(line.data(), static_cast<std::streamsize>(line.size()));
    stream_.flush();
}

void Logger::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

}  // namespace campaign_completion
