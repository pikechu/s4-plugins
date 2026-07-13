#pragma once

#include <string>
#include <string_view>

namespace campaign_completion {

enum class SuMapReadStatus {
    Success,
    SuTableMissing,
    GameTableMissing,
    FunctionMissing,
    CallError,
    NonString,
    Empty,
    TooLong,
    ControlCharacter,
    EncodingFailure,
    AbsolutePath,
    PathTraversal,
    ValueConflict,
    StaleGeneration,
    Timeout,
};

struct ValidatedSuValue {
    SuMapReadStatus status = SuMapReadStatus::Empty;
    std::string displayUtf8;
    std::wstring normalized;

    explicit operator bool() const noexcept {
        return status == SuMapReadStatus::Success;
    }
};

ValidatedSuValue ValidateSuMapName(std::string_view bytes);
ValidatedSuValue ValidateSuRelativePath(std::string_view bytes);
std::string_view SuMapReadStatusName(SuMapReadStatus status) noexcept;

}  // namespace campaign_completion
