#include "identity/SuMapValue.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunSuMapValueTests() {
    using campaign_completion::SuMapReadStatus;
    using campaign_completion::SuMapReadStatusName;
    using campaign_completion::ValidateSuMapName;
    using campaign_completion::ValidateSuRelativePath;

    const auto name = ValidateSuMapName("Aeneas");
    Require(name.status == SuMapReadStatus::Success,
            "plain map name is accepted");
    Require(name.normalized == L"Aeneas", "map name keeps its wide value");
    Require(name.displayUtf8 == "Aeneas", "map name keeps its display value");
    Require(ValidateSuMapName("").status == SuMapReadStatus::Empty,
            "empty map name is rejected");
    Require(ValidateSuMapName(std::string(513u, 'a')).status ==
                SuMapReadStatus::TooLong,
            "513-byte map name is rejected");
    Require(ValidateSuMapName("bad\nname").status ==
                SuMapReadStatus::ControlCharacter,
            "line-feed trace injection is rejected");
    Require(ValidateSuMapName("bad\rname").status ==
                SuMapReadStatus::ControlCharacter,
            "carriage-return trace injection is rejected");

    const auto relative =
        ValidateSuRelativePath("Maps//Single/./Aeneas.map");
    Require(relative.status == SuMapReadStatus::Success,
            "relative map path is accepted");
    Require(relative.normalized == L"Maps\\Single\\Aeneas.map",
            "relative map path is normalized");
    Require(relative.displayUtf8 == "Maps\\Single\\Aeneas.map",
            "normalized relative path is emitted as UTF-8");

    for (const auto* absolute : {
             "C:\\Maps\\Aeneas.map", "C:Aeneas.map", "\\Aeneas.map",
             "\\\\server\\share\\a.map", "\\\\?\\C:\\a.map"}) {
        Require(ValidateSuRelativePath(absolute).status ==
                    SuMapReadStatus::AbsolutePath,
                "absolute, drive-relative, UNC, and device paths are rejected");
    }
    Require(ValidateSuRelativePath("Maps\\..\\a.map").status ==
                SuMapReadStatus::PathTraversal,
            "parent path traversal is rejected");
    Require(ValidateSuRelativePath("Maps\\.\\Aeneas").status ==
                SuMapReadStatus::Success,
            "map suffix is not required");

    Require(SuMapReadStatusName(SuMapReadStatus::Success) == "success",
            "success status has a stable trace name");
    Require(SuMapReadStatusName(SuMapReadStatus::PathTraversal) ==
                "path-traversal",
            "path traversal status has a stable trace name");
    return 0;
}
