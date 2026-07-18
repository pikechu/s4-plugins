#include "identity/MapPathValidator.h"

#include <windows.h>

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

void WriteFile(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "map";
    Require(output.good(), "map fixture must be writable");
}

}  // namespace

int RunMapPathValidatorTests() {
    using campaign_completion::IsFinalPathBeneath;
    using campaign_completion::MapPathFailure;
    using campaign_completion::MapPathValidator;

    const auto fixtureBase =
        std::filesystem::temp_directory_path() /
        (L"CampaignCompletionMapPath-" +
         std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(GetTickCount64()));
    const auto gameRoot = fixtureBase / L"Game";
    const auto mapRoot = gameRoot / L"Map";
    const auto userRoot = mapRoot / L"User";
    const auto outsideRoot = fixtureBase / L"Outside";
    std::filesystem::create_directories(userRoot);
    std::filesystem::create_directories(outsideRoot);
    WriteFile(userRoot / L"Stable.map");
    WriteFile(outsideRoot / L"Outside.map");
    std::filesystem::create_directories(userRoot / L"Folder.map");

    const MapPathValidator validator(gameRoot);

    const auto accepted = validator.ValidateWide(L"Map/User/Stable.map");
    Require(accepted.failure == MapPathFailure::None,
            "valid map path accepted");
    Require(accepted.relativePath == "Map\\User\\Stable.map",
            "accepted identity is normalized game-relative UTF-8");

    const auto mixedCase = validator.ValidateWide(L"mAp/uSeR/sTaBlE.MaP");
    Require(mixedCase.failure == MapPathFailure::None,
            "case-insensitive lookup accepted");
    Require(mixedCase.relativePath == "mAp\\uSeR\\sTaBlE.MaP",
            "case is preserved while separators normalize");

    Require(validator.ValidateWide(L"").failure == MapPathFailure::Empty,
            "empty input rejected");
    Require(validator.ValidateWide(L"Map/User/Bad\x001f.map").failure ==
                MapPathFailure::ControlCharacter,
            "control character rejected");
    Require(validator.ValidateWide(L"C:\\Map\\User\\Stable.map").failure ==
                MapPathFailure::RootedPath,
            "drive-qualified path rejected");
    Require(validator.ValidateWide(L"\\\\server\\share\\Stable.map").failure ==
                MapPathFailure::RootedPath,
            "UNC path rejected");
    Require(validator.ValidateWide(L"\\Map\\User\\Stable.map").failure ==
                MapPathFailure::RootedPath,
            "root-relative path rejected");
    Require(validator.ValidateWide(L"Map\\.\\Stable.map").failure ==
                MapPathFailure::Traversal,
            "dot component rejected");
    Require(validator.ValidateWide(L"Map\\User\\..\\Stable.map").failure ==
                MapPathFailure::Traversal,
            "parent traversal rejected");
    Require(validator.ValidateWide(L"Maps\\User\\Stable.map").failure ==
                MapPathFailure::WrongRoot,
            "non-Map root rejected");
    Require(validator.ValidateWide(L"Map\\User\\Stable.txt").failure ==
                MapPathFailure::WrongExtension,
            "non-map extension rejected");
    Require(validator.ValidateWide(L"Map\\User\\Folder.map").failure ==
                MapPathFailure::NotRegularFile,
            "directory rejected");
    Require(validator.ValidateWide(L"Map\\User\\Missing.map").failure ==
                MapPathFailure::CandidateUnavailable,
            "missing map rejected");

    const auto escapeLink = mapRoot / L"Escape";
    constexpr DWORD unprivilegedCreate = 0x2u;
    BOOL linked = CreateSymbolicLinkW(
        escapeLink.c_str(), outsideRoot.c_str(),
        SYMBOLIC_LINK_FLAG_DIRECTORY | unprivilegedCreate);
    if (linked == FALSE && GetLastError() == ERROR_INVALID_PARAMETER) {
        linked = CreateSymbolicLinkW(escapeLink.c_str(), outsideRoot.c_str(),
                                     SYMBOLIC_LINK_FLAG_DIRECTORY);
    }
    Require(linked != FALSE, "directory symlink fixture must be created");
    Require(validator.ValidateWide(L"Map\\Escape\\Outside.map").failure ==
                MapPathFailure::OutsideMapRoot,
            "final-handle reparse escape rejected");

    Require(IsFinalPathBeneath(L"\\\\?\\C:\\Game\\Map",
                               L"\\\\?\\c:\\game\\map\\User\\A.map"),
            "case-insensitive descendant accepted");
    Require(!IsFinalPathBeneath(L"\\\\?\\C:\\Game\\Map",
                                L"\\\\?\\C:\\Game\\MapSibling\\A.map"),
            "component-boundary sibling rejected");
    Require(!IsFinalPathBeneath(L"\\\\?\\C:\\Game\\Map",
                                L"\\\\?\\C:\\Game\\Map"),
            "root itself is not a map descendant");

    std::filesystem::remove_all(fixtureBase);
    return 0;
}
