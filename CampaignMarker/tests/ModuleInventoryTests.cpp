#include "diagnostics/ModuleInventory.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::wstring Lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool ContainsModule(const std::vector<campaign_completion::ModuleInfo>& modules,
                    const std::wstring& name) {
    const auto expected = Lowercase(name);
    return std::any_of(modules.begin(), modules.end(), [&](const auto& module) {
        return Lowercase(module.name) == expected;
    });
}

}  // namespace

int RunModuleInventoryTests() {
    wchar_t executablePath[MAX_PATH]{};
    const auto pathLength = GetModuleFileNameW(
        nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
    Require(pathLength > 0, "test executable path must be available");

    const auto modules = campaign_completion::EnumerateLoadedModules();
    const auto executableName = std::filesystem::path(executablePath).filename().wstring();
    Require(ContainsModule(modules, executableName),
            "module inventory must contain the current executable");
    Require(ContainsModule(modules, L"kernel32.dll"),
            "module inventory must contain kernel32.dll");

    const auto hashPath = std::filesystem::temp_directory_path() /
                          L"CampaignCompletionSha256Test.bin";
    {
        std::ofstream output(hashPath, std::ios::binary | std::ios::trunc);
        output << "abc";
    }
    const auto hash = campaign_completion::Sha256File(hashPath);
    Require(hash.has_value(), "SHA-256 must be produced for a readable file");
    Require(*hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "SHA-256 must match the standard abc test vector");
    std::filesystem::remove(hashPath);

    campaign_completion::ModuleInfo unapproved{};
    unapproved.name = L"S4_Main.exe";
    unapproved.version = "2.50.1516.0";
    unapproved.sha256 = std::string(64, '0');
    Require(campaign_completion::CheckTargetExecutable(unapproved) ==
                campaign_completion::CompatibilityResult::HashMismatch,
            "an unapproved executable hash must fail closed");
    return 0;
}
