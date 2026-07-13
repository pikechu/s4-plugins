#include "identity/FixedMapIdentityProbe.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::CapturedWidePath Capture(const wchar_t* path) {
    return {path, campaign_completion::WideCaptureFailure::None};
}

bool Contains(const std::vector<std::string>& records,
              const std::string& fragment) {
    return std::any_of(records.begin(), records.end(), [&](const auto& record) {
        return record.find(fragment) != std::string::npos;
    });
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

}  // namespace

int RunFixedMapIdentityProbeTests() {
    using campaign_completion::FixedMapIdentityProbe;
    using campaign_completion::FixedMapListKind;

    const auto fixture = std::filesystem::temp_directory_path() /
                         (L"FixedMapProbe-" +
                          std::to_wstring(GetCurrentProcessId()) + L"-" +
                          std::to_wstring(GetTickCount64()));
    const auto gameRoot = fixture / L"Game";
    std::filesystem::create_directories(gameRoot / L"Map" / L"User");
    for (const auto* name : {L"A.map", L"B.map", L"Multi.map", L"Custom.map"}) {
        std::ofstream output(gameRoot / L"Map" / L"User" / name,
                             std::ios::binary | std::ios::trunc);
        output << "map";
    }

    std::vector<std::string> records;
    std::vector<std::string> trace;
    FixedMapIdentityProbe probe(
        gameRoot,
        [&](std::string record) { records.push_back(std::move(record)); },
        [&](std::string record) { trace.push_back(std::move(record)); });

    probe.ObserveListKind(FixedMapListKind::Single, 100u);
    probe.ObserveMapInit(200u);
    Require(Contains(trace, "adapter-entered=0"),
            "no adapter entry is explicit");
    Require(Contains(trace, "map-init-association=no-candidate"),
            "no entry remains no-candidate");

    probe.ObserveListKind(FixedMapListKind::Single, 300u);
    probe.Observe({{}, campaign_completion::WideCaptureFailure::NullObject},
                  1u, 400u);
    probe.ObserveMapInit(500u);
    Require(Contains(trace, "adapter-entered=1"),
            "failed capture proves adapter entry");
    Require(Contains(trace, "wide-capture=null-object"),
            "exact wide failure is retained");
    Require(Contains(trace, "map-init-association=wide-null-object"),
            "wide failure owns the association result");

    probe.ObserveListKind(FixedMapListKind::Single, 1000u);
    probe.Observe(Capture(L"Map\\User\\A.map"), 1u, 2000u);
    probe.ObserveMapInit(3000u);
    Require(Contains(records, "identity confirmed list_kind=single"),
            "single identity confirmed");
    Require(Contains(records, "path=Map\\User\\A.map sequence=1 epoch=3"),
            "single record has stable identity fields");

    probe.ObserveListKind(FixedMapListKind::Multiplayer, 4000u);
    probe.Observe(Capture(L"Map\\User\\Multi.map"), 2u, 5000u);
    probe.ObserveMapInit(6000u);
    probe.ObserveListKind(FixedMapListKind::Custom, 7000u);
    const auto beforeRepeated = records.size();
    probe.ObserveListKind(FixedMapListKind::Custom, 7100u);
    probe.ObserveListKind(FixedMapListKind::Custom, 7200u);
    Require(records.size() == beforeRepeated,
            "repeated identical list callbacks emit no duplicate");
    probe.Observe(Capture(L"Map\\User\\Custom.map"), 3u, 8000u);
    probe.ObserveMapInit(9000u);
    Require(Contains(records, "list_kind=multiplayer path=Map\\User\\Multi.map"),
            "multiplayer identity remains distinct");
    Require(Contains(records, "list_kind=custom path=Map\\User\\Custom.map"),
            "custom identity remains distinct");

    probe.ObserveBack();
    probe.ObserveMapInit(9300u);
    Require(Contains(records, "identity unknown reason=no-active-epoch"),
            "Back invalidates epoch");

    for (const DWORD page : {1u, 7u}) {
        probe.ObserveListKind(FixedMapListKind::Single, 10000u + page);
        probe.Observe(Capture(L"Map\\User\\A.map"), page, 10100u + page);
        probe.ObserveTopLevelPage(page);
        probe.ObserveMapInit(10200u + page);
        Require(records.back().find("reason=no-active-epoch") != std::string::npos,
                "top-level page invalidates epoch");
    }

    for (const DWORD page : {26u, 30u, 37u}) {
        probe.ObserveListKind(FixedMapListKind::Single, 11000u + page);
        probe.Observe(Capture(L"Map\\User\\A.map"), page, 11100u + page);
        probe.ObserveTopLevelPage(page);
        probe.ObserveMapInit(11200u + page);
        Require(records.back().find("identity confirmed") != std::string::npos,
                "lobby and mission pages preserve epoch");
    }

    probe.ObserveListKind(FixedMapListKind::Single, 20000u);
    probe.Observe(Capture(L"Map\\User\\Missing.map"), 20u, 20100u);
    probe.ObserveMapInit(20200u);
    Require(records.back().find("identity unknown reason=path-") !=
                std::string::npos,
            "invalid or missing path has stable unknown reason");

    probe.ObserveListKind(FixedMapListKind::Single, 21000u);
    probe.Observe(Capture(L"Map\\User\\A.map"), 21u, 21100u);
    probe.Observe(Capture(L"Map\\User\\B.map"), 22u, 21200u);
    probe.ObserveMapInit(21300u);
    Require(records.back().find("reason=ambiguous") != std::string::npos,
            "different paths report ambiguity");

    probe.ObserveListKind(FixedMapListKind::Single, 22000u);
    probe.Observe(Capture(L"Map\\User\\A.map"), 23u, 22100u);
    probe.ObserveMapInit(22100u + campaign_completion::kCaptureLeaseMs);
    Require(records.back().find("reason=expired") != std::string::npos,
            "exact capture expiry reports unknown");

    probe.ObserveListKind(FixedMapListKind::Single, 23000u);
    for (std::uint64_t index = 0; index < 9u; ++index) {
        probe.Observe(Capture((L"Map\\User\\Missing" +
                               std::to_wstring(index) + L".map").c_str()),
                      30u + index, 23100u + index);
    }
    probe.ObserveMapInit(23200u);
    Require(records.back().find("reason=overflow") != std::string::npos,
            "raw capture queue overflow fails closed");

    const auto beforeDisable = records.size();
    probe.Disable();
    probe.ObserveListKind(FixedMapListKind::Single, 24000u);
    probe.Observe(Capture(L"Map\\User\\A.map"), 40u, 24100u);
    probe.ObserveMapInit(24200u);
    Require(records.size() == beforeDisable + 1u,
            "Disable emits once and permanently rejects observations");

    std::string all;
    for (const auto& record : records) {
        all += record;
        all += '\n';
    }
    const auto lowered = Lowercase(all);
    Require(all.find("0x") == std::string::npos,
            "probe output contains no raw pointer");
    Require(all.find(gameRoot.string()) == std::string::npos,
            "probe output contains no absolute game directory");
    Require(lowered.find("victory") == std::string::npos,
            "probe output contains no victory claim");
    Require(lowered.find("completion") == std::string::npos,
            "probe output contains no completion claim");
    Require(lowered.find("marker") == std::string::npos,
            "probe output contains no marker claim");

    for (const auto& line : trace) {
        const bool allowed =
            line.rfind("adapter-entered=", 0) == 0 ||
            line.rfind("wide-capture=", 0) == 0 ||
            line.rfind("path-validation=", 0) == 0 ||
            line.rfind("map-init-association=", 0) == 0;
        Require(allowed, "probe trace uses only approved record prefixes");
    }

    std::filesystem::remove_all(fixture);
    return 0;
}
