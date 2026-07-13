#include "diagnostics/CaptureTrace.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

}  // namespace

int RunCaptureTraceTests() {
    using campaign_completion::CaptureTrace;
    using campaign_completion::IsCaptureTraceRootAdmitted;

    const auto root = std::filesystem::temp_directory_path() /
                      (L"CampaignCaptureTrace-" +
                       std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(root);

    CaptureTrace first;
    Require(first.Open(root, 4242u), "existing directory is admitted");
    const auto firstPath = first.path();
    Require(firstPath.filename() == L"capture-trace-4242.log",
            "first process trace uses the base name");
    Require(first.Write("adapter-entered=1"), "adapter line accepted");
    Require(first.Write("wide-capture=null-object"),
            "wide line accepted");
    Require(first.Write("path-validation=success"),
            "path line accepted");
    Require(first.Write("map-init-association=wide-null-object"),
            "association line accepted");
    Require(first.Write("controlled-stop-flush=success"),
            "stop line accepted");
    Require(!first.Write("module name=S4_Main.exe"),
            "non-allowlisted line rejected");
    first.Close();
    const auto firstContents = ReadAll(firstPath);

    CaptureTrace second;
    Require(second.Open(root, 4242u), "same pid obtains a unique file");
    Require(second.path().filename() == L"capture-trace-4242-1.log",
            "existing file is never overwritten");
    second.Close();

    Require(ReadAll(firstPath) == firstContents,
            "opening a second trace leaves the first byte-identical");
    Require(firstContents.find("module name=") == std::string::npos,
            "rejected line never reaches disk");

    CaptureTrace missing;
    Require(!missing.Open(root / L"missing", 1u),
            "missing parent is not created");
    CaptureTrace fileRoot;
    Require(!fileRoot.Open(firstPath, 1u),
            "regular file is not a trace root");

    Require(IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, true),
            "plain final-resolved directory admitted");
    Require(!IsCaptureTraceRootAdmitted(
                FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT,
                true),
            "reparse directory rejected");
    Require(!IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_NORMAL, true),
            "non-directory attributes rejected");
    Require(!IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, false),
            "missing final-handle path rejected");

    std::filesystem::remove_all(root);
    return 0;
}
