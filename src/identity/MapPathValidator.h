#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace campaign_completion {

enum class MapPathFailure {
    None,
    Empty,
    ControlCharacter,
    RootedPath,
    Traversal,
    WrongRoot,
    WrongExtension,
    MapRootUnavailable,
    CandidateUnavailable,
    NotRegularFile,
    FinalPathUnavailable,
    OutsideMapRoot,
    Utf8ConversionFailed,
};

struct MapPathResult final {
    std::string relativePath;
    MapPathFailure failure = MapPathFailure::None;

    explicit operator bool() const noexcept {
        return failure == MapPathFailure::None;
    }
};

bool IsFinalPathBeneath(std::wstring_view root,
                        std::wstring_view candidate) noexcept;

class MapPathValidator final {
public:
    explicit MapPathValidator(std::filesystem::path gameRoot);

    MapPathResult ValidateWide(std::wstring_view raw) const;

private:
    std::filesystem::path gameRoot_;
};

}  // namespace campaign_completion
