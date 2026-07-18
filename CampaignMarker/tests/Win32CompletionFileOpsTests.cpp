#include "completion/Win32CompletionFileOps.h"

#include <windows.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunWin32CompletionFileOpsTests() {
    using namespace campaign_completion;

    const auto root = std::filesystem::temp_directory_path() /
        (L"CampaignCompletionStoreTests-" +
         std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(GetTickCount64()));
    const auto main = root / L"completed_maps.json";
    const auto temporary = root / L"completed_maps.json.tmp";
    const auto backup = root / L"completed_maps.json.bak";
    std::filesystem::create_directories(root);
    try {
        Win32CompletionFileOps files;
        const auto missing = files.ReadBounded(main, 1024u);
        Require(missing.status == CompletionFileStatus::Missing,
                "missing real file must be distinguished");

        Require(files.WriteTemporaryAndFlush(temporary, "first").success,
                "real temporary write and flush must succeed");
        Require(files.MoveFirstWriteThrough(temporary, main).success,
                "real first write-through move must succeed");
        Require(files.ReadBounded(main, 1024u).bytes == "first",
                "first moved bytes must be readable");

        Require(files.WriteTemporaryAndFlush(temporary, "second").success,
                "second temporary write must succeed");
        Require(files.ReplaceWithBackup(main, temporary, backup).success,
                "real atomic replacement must succeed");
        Require(files.ReadBounded(main, 1024u).bytes == "second" &&
                    files.ReadBounded(backup, 1024u).bytes == "first",
                "replacement must publish new main and preserve old backup");

        Require(files.WriteTemporaryAndFlush(temporary, "third").success,
                "third temporary write must succeed");
        Require(files.ReplaceWithBackup(main, temporary, backup).success,
                "replacement must update an existing backup");
        Require(files.ReadBounded(main, 1024u).bytes == "third" &&
                    files.ReadBounded(backup, 1024u).bytes == "second",
                "existing backup must become the immediately previous main");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        throw;
    }
    std::error_code error;
    std::filesystem::remove_all(root, error);
    Require(!error, "real-file test directory cleanup must succeed");
    return 0;
}
