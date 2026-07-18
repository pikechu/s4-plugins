#include "completion/ReadOnlyCompletionSource.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class ReadOnlyFixture final : public campaign_completion::ICompletionFileOps {
public:
    campaign_completion::CompletionReadResult ReadBounded(
        const std::filesystem::path&, std::size_t) noexcept override {
        ++reads;
        return read;
    }
    campaign_completion::CompletionWriteResult WriteTemporaryAndFlush(
        const std::filesystem::path&, std::string_view) noexcept override {
        ++writes;
        return {};
    }
    campaign_completion::CompletionWriteResult ReplaceWithBackup(
        const std::filesystem::path&, const std::filesystem::path&,
        const std::filesystem::path&) noexcept override {
        ++writes;
        return {};
    }
    campaign_completion::CompletionWriteResult MoveFirstWriteThrough(
        const std::filesystem::path&,
        const std::filesystem::path&) noexcept override {
        ++writes;
        return {};
    }

    campaign_completion::CompletionReadResult read{};
    std::size_t reads = 0u;
    std::size_t writes = 0u;
};

}  // namespace

int RunReadOnlyCompletionSourceTests() {
    using namespace campaign_completion;

    CompletionDatabaseSnapshot snapshot{};
    const auto encoded = EncodeCompletionJson(snapshot);
    Require(encoded.has_value(), "empty database fixture encodes");
    ReadOnlyFixture files;
    files.read = {CompletionFileStatus::Success, *encoded, ERROR_SUCCESS};
    ReadOnlyCompletionSource source(files, L"main");
    const auto loaded = source.Load();
    Require(loaded.status == ReadOnlyCompletionStatus::Loaded &&
                loaded.snapshot.records.empty() && files.reads == 1u &&
                files.writes == 0u,
            "bounded source reads main exactly once and never calls a writer");

    files.read = {CompletionFileStatus::Missing, {}, ERROR_FILE_NOT_FOUND};
    ReadOnlyCompletionSource missing(files, L"main");
    Require(missing.Load().status == ReadOnlyCompletionStatus::Missing &&
                files.writes == 0u,
            "missing main fails closed without creating a database");

    files.read = {CompletionFileStatus::Success, "{broken", ERROR_SUCCESS};
    ReadOnlyCompletionSource malformed(files, L"main");
    Require(malformed.Load().status == ReadOnlyCompletionStatus::Malformed &&
                files.writes == 0u,
            "malformed main fails closed without normalization or backup use");

    return 0;
}
