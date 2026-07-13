#include "diagnostics/Phase3Trace.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

}  // namespace

int RunPhase3TraceTests() {
    using campaign_completion::Phase3Trace;
    using campaign_completion::Phase3TraceChannel;

    const auto root = std::filesystem::temp_directory_path() /
                      (L"CampaignPhase3Trace-" +
                       std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(root);

    Phase3Trace first;
    Require(first.Open(root, 42u), "admitted project root opens");
    const auto firstDirectory = first.directory();
    Require(firstDirectory.filename() == L"phase-3-session-42",
            "first trace uses the base session directory");
    Require(first.Write(Phase3TraceChannel::Origin,
                        "origin-source=single-player-multiplayer-map"),
            "origin allowlist accepts source");
    Require(first.Write(Phase3TraceChannel::Origin,
                        "origin-eligibility=eligible"),
            "origin allowlist accepts eligibility");
    Require(first.Write(
                Phase3TraceChannel::Origin,
                "origin-refinement=session-3;source-random-map;eligibility-eligible;ui-hidden"),
            "origin allowlist accepts bounded identity refinement");
    Require(first.Write(Phase3TraceChannel::Identity,
                        "identity-association=confirmed"),
            "identity allowlist accepts association");
    Require(first.Write(Phase3TraceChannel::Identity,
                        "su-map-relative=Map\\Singleplayer\\Aeneas.map"),
            "identity allowlist accepts a relative map path");
    Require(first.Write(
                Phase3TraceChannel::SettlementUi,
                "settlement-capture=session-1;features=3;truncated=0"),
            "settlement allowlist accepts summary");
    Require(first.Write(
                Phase3TraceChannel::SettlementUi,
                "settlement-feature=gfx-4;texture-120;value-55;x-10;y-20"),
            "settlement allowlist accepts numeric feature");
    Require(first.Write(Phase3TraceChannel::NativeEvent,
                        "native-subscription=attached"),
            "native channel accepts subscription state");
    Require(first.Write(
                Phase3TraceChannel::NativeEvent,
                "native-event=session-7;event-id=609;local-result=won;wparam=1;lparam=0;game-tick=123"),
            "native channel accepts bounded numeric event evidence");
    Require(first.Write(Phase3TraceChannel::NativeEvent,
                        "native-event-duplicates=session-7;count=0"),
            "native channel accepts duplicate count");
    Require(first.Write(Phase3TraceChannel::NativeEvent,
                        "native-event-orphans=count=1"),
            "native channel accepts orphan count");
    Require(first.Write(Phase3TraceChannel::Decision,
                        "diagnostic-result=calibration-only"),
            "decision channel accepts calibration-only status");
    Require(first.Write(Phase3TraceChannel::Decision,
                        "controlled-stop-flush=success"),
            "decision channel accepts controlled stop");

    Require(!first.Write(Phase3TraceChannel::SettlementUi,
                         "chat=private text"),
            "unrelated text is rejected");
    Require(!first.Write(Phase3TraceChannel::Origin,
                         "origin-status=C:\\secret\\game.sav"),
            "absolute save path is rejected");
    Require(!first.Write(Phase3TraceChannel::Identity,
                         "su-map-relative=\\\\server\\share\\a.map"),
            "UNC map path is rejected");
    Require(!first.Write(Phase3TraceChannel::Decision,
                         "victory-confirmed"),
            "Phase 3A cannot claim a victory decision");
    Require(!first.Write(Phase3TraceChannel::NativeEvent,
                         "native-event=0x261"),
            "native channel rejects hexadecimal pointer-like syntax");
    Require(!first.Write(Phase3TraceChannel::NativeEvent,
                         "victory-confirmed"),
            "native channel rejects a completion claim");
    Require(!first.Write(Phase3TraceChannel::Origin,
                         "origin-source=bad\nvalue"),
            "line breaks are rejected");
    Require(!first.Write(Phase3TraceChannel::Origin,
                         "origin-status=" + std::string(1025u, 'a')),
            "oversized records are rejected");
    first.Close();

    for (const auto* name : {"origin.trace", "identity.trace",
                             "settlement-ui.trace", "native-event.trace",
                             "decision.trace"}) {
        Require(std::filesystem::exists(firstDirectory / name),
                "all five bounded channels exist");
    }
    Require(ReadAll(firstDirectory / "origin.trace").find("private text") ==
                std::string::npos,
            "rejected text never reaches disk");

    Phase3Trace second;
    Require(second.Open(root, 42u), "same pid obtains a unique directory");
    Require(second.directory().filename() == L"phase-3-session-42-1",
            "existing session directory is never overwritten");
    second.Close();

    Phase3Trace missing;
    Require(!missing.Open(root / L"missing", 1u),
            "missing project root is not created");
    Phase3Trace fileRoot;
    Require(!fileRoot.Open(firstDirectory / L"origin.trace", 1u),
            "regular file is not admitted as a root");

    std::filesystem::remove_all(root);
    return 0;
}
