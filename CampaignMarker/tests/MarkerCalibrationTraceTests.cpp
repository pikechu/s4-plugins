#include "diagnostics/MarkerCalibrationTrace.h"

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

int RunMarkerCalibrationTraceTests() {
    using campaign_completion::IsMarkerCalibrationRecordAllowed;
    using campaign_completion::IsMarkerCalibrationRootAdmitted;
    using campaign_completion::kMarkerCalibrationMaximumBytes;
    using campaign_completion::MarkerCalibrationTrace;

    Require(IsMarkerCalibrationRecordAllowed(
                "marker-state=pages-fixed;list-single"),
            "bounded marker state is admitted");
    Require(IsMarkerCalibrationRecordAllowed(
                "marker-candidate=sequence-7;generation-2;list-single;"
                "name-Aeneas;rect-100,120,300,24;surface-800,600;"
                "value-link-2422;container-1;text-style-9;match-unique"),
            "candidate feature record is admitted");
    Require(IsMarkerCalibrationRecordAllowed(
                "marker-frame=sequence-8;generation-2;page-25;pillarbox-0;"
                "candidate-first-7;candidate-last-7"),
            "candidate frame boundary is admitted");
    Require(!IsMarkerCalibrationRecordAllowed(
                "marker-candidate=0x1234"),
            "pointer-shaped token is rejected");
    Require(!IsMarkerCalibrationRecordAllowed(
                "marker-candidate=name-Aeneas\nmodule name=S4_Main.exe"),
            "newline and unrelated module data are rejected");
    Require(!IsMarkerCalibrationRecordAllowed(
                "marker-candidate=path-F:\\Games\\Aeneas.map"),
            "absolute path is rejected");

    Require(IsMarkerCalibrationRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, true),
            "plain resolved directory is admitted");
    Require(!IsMarkerCalibrationRootAdmitted(
                FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT,
                true),
            "reparse directory is rejected");
    Require(!IsMarkerCalibrationRootAdmitted(FILE_ATTRIBUTE_NORMAL, true),
            "regular file is rejected as a root");
    Require(!IsMarkerCalibrationRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, false),
            "unresolved final path is rejected");

    const auto root = std::filesystem::temp_directory_path() /
                      (L"CampaignMarkerCalibration-" +
                       std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(root);

    MarkerCalibrationTrace first;
    Require(first.Open(root, 4242u), "existing trace root is admitted");
    const auto firstPath = first.path();
    Require(firstPath.filename() == L"marker-calibration-pid-4242.trace",
            "first trace uses the PID base name");
    Require(first.Write("marker-state=pages-fixed;list-single"),
            "valid state record is written");
    const auto firstContents = ReadAll(firstPath);

    MarkerCalibrationTrace second;
    Require(second.Open(root, 4242u), "same PID obtains a unique trace");
    Require(second.path().filename() ==
                L"marker-calibration-pid-4242-1.trace",
            "existing trace is never overwritten");
    second.Close();
    Require(ReadAll(firstPath) == firstContents,
            "opening another trace preserves the first file");

    std::string largeRecord = "marker-state=";
    largeRecord.append(1900u, 'a');
    while (first.Write(largeRecord)) {
    }
    first.Close();
    Require(std::filesystem::file_size(firstPath) <=
                kMarkerCalibrationMaximumBytes,
            "trace never exceeds the 256 KiB cap");
    Require(!first.Write("marker-state=after-close"),
            "closed trace rejects writes");

    MarkerCalibrationTrace missing;
    Require(!missing.Open(root / L"missing", 1u),
            "missing root is not created");
    MarkerCalibrationTrace fileRoot;
    Require(!fileRoot.Open(firstPath, 1u),
            "regular file cannot be a trace root");

    std::filesystem::remove_all(root);
    return 0;
}
