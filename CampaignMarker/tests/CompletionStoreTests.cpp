#include "completion/CompletionStore.h"
#include "CompletionDatabaseFixtures.h"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::CompletionRecord Record(std::string suffix) {
    using namespace campaign_completion;
    CompletionRecord record{};
    record.stableId = "map:map\\user\\" + suffix + ".map";
    record.relativeId = "Map\\User\\" + suffix + ".map";
    record.displayName = suffix;
    record.mapKind = CompletionMapKind::Fixed;
    record.launchSource = LaunchSource::SinglePlayerCustomMap;
    record.completedAtUtc = "2026-07-14T01:57:38Z";
    return record;
}

campaign_completion::CompletionRecord LegacyRecord(
    std::string suffix,
    campaign_completion::LaunchSource source) {
    auto record = Record(std::move(suffix));
    record.launchSource = source;
    return record;
}

campaign_completion::CompletionRecord RecordAt(
    std::string relative,
    std::string display,
    campaign_completion::LaunchSource source) {
    using namespace campaign_completion;
    CompletionRecord record{};
    record.relativeId = std::move(relative);
    const auto wide = StrictUtf8ToWide(record.relativeId);
    if (!wide.has_value()) {
        throw std::runtime_error("valid fixture relative path did not decode");
    }
    const auto stable = BuildStableMapId(*wide);
    if (!stable.has_value()) {
        throw std::runtime_error("valid fixture relative path did not normalize");
    }
    record.stableId = *stable;
    record.displayName = std::move(display);
    record.mapKind = CompletionMapKind::Fixed;
    record.launchSource = source;
    record.completedAtUtc = "2026-07-14T01:57:38Z";
    return record;
}

std::string Database(const campaign_completion::CompletionRecord& record) {
    campaign_completion::CompletionDatabaseSnapshot snapshot{};
    snapshot.records.push_back(record);
    const auto encoded = campaign_completion::EncodeCompletionJson(snapshot);
    if (!encoded.has_value()) {
        throw std::runtime_error("valid test record did not encode");
    }
    return *encoded;
}

class FakeFileOps final : public campaign_completion::ICompletionFileOps {
public:
    using path = std::filesystem::path;

    campaign_completion::CompletionReadResult ReadBounded(
        const path& value, std::size_t limit) noexcept override {
        ++readCalls;
        if (failedRead == value) {
            return {campaign_completion::CompletionFileStatus::Failed, {},
                    ERROR_ACCESS_DENIED};
        }
        const auto found = files.find(value);
        if (found == files.end()) {
            return {campaign_completion::CompletionFileStatus::Missing, {},
                    ERROR_FILE_NOT_FOUND};
        }
        if (found->second.size() > limit) {
            return {campaign_completion::CompletionFileStatus::Failed, {},
                    ERROR_FILE_TOO_LARGE};
        }
        if (corruptTemporaryRead && value == temporary) {
            return {campaign_completion::CompletionFileStatus::Success,
                    "{broken", ERROR_SUCCESS};
        }
        return {campaign_completion::CompletionFileStatus::Success,
                found->second, ERROR_SUCCESS};
    }

    campaign_completion::CompletionWriteResult WriteTemporaryAndFlush(
        const path& value, std::string_view bytes) noexcept override {
        ++writeCalls;
        if (writeFailure ==
            campaign_completion::CompletionTransactionStage::WriteTemporary) {
            return {false, writeFailure, ERROR_WRITE_FAULT};
        }
        files[value] = std::string(bytes);
        if (writeFailure ==
            campaign_completion::CompletionTransactionStage::FlushTemporary) {
            return {false, writeFailure, ERROR_WRITE_FAULT};
        }
        return {true,
                campaign_completion::CompletionTransactionStage::None,
                ERROR_SUCCESS};
    }

    campaign_completion::CompletionWriteResult ReplaceWithBackup(
        const path& main, const path& temporaryPath,
        const path& backup) noexcept override {
        ++replaceCalls;
        if (replaceFailure !=
            campaign_completion::CompletionTransactionStage::None) {
            return {false, replaceFailure, ERROR_UNABLE_TO_MOVE_REPLACEMENT};
        }
        files[backup] = files[main];
        files[main] = files[temporaryPath];
        files.erase(temporaryPath);
        return {true,
                campaign_completion::CompletionTransactionStage::None,
                ERROR_SUCCESS};
    }

    campaign_completion::CompletionWriteResult MoveFirstWriteThrough(
        const path& temporaryPath, const path& main) noexcept override {
        ++moveCalls;
        if (moveFailure) {
            return {false,
                    campaign_completion::CompletionTransactionStage::MoveFirst,
                    ERROR_UNABLE_TO_MOVE_REPLACEMENT};
        }
        files[main] = files[temporaryPath];
        files.erase(temporaryPath);
        return {true,
                campaign_completion::CompletionTransactionStage::None,
                ERROR_SUCCESS};
    }

    void ResetOperationCounts() noexcept {
        readCalls = 0u;
        writeCalls = 0u;
        replaceCalls = 0u;
        moveCalls = 0u;
    }

    std::map<path, std::string> files;
    path failedRead;
    path temporary = L"temporary";
    campaign_completion::CompletionTransactionStage writeFailure =
        campaign_completion::CompletionTransactionStage::None;
    campaign_completion::CompletionTransactionStage replaceFailure =
        campaign_completion::CompletionTransactionStage::None;
    bool corruptTemporaryRead = false;
    bool moveFailure = false;
    std::size_t readCalls = 0u;
    std::size_t writeCalls = 0u;
    std::size_t replaceCalls = 0u;
    std::size_t moveCalls = 0u;
};

campaign_completion::CompletionStore MakeStore(FakeFileOps& files) {
    return campaign_completion::CompletionStore(
        files, L"main", L"temporary", L"backup");
}

}  // namespace

int RunCompletionStoreTests() {
    using namespace campaign_completion;

    {
        FakeFileOps files;
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode == CompletionStoreMode::WritableEmpty &&
                    load.recordCount == 0u,
                "no main and no backup must be writable empty");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = Database(Record("a"));
        files.files[L"backup"] = "{broken";
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode == CompletionStoreMode::WritableLoaded &&
                    load.recordCount == 1u && files.writeCalls == 0u &&
                    files.replaceCalls == 0u,
                "valid main must remain writable despite invalid backup");
    }
    {
        FakeFileOps files;
        const auto mainBefore =
            test_fixtures::LiveThreeRecordDatabase();
        const auto backupBefore = Database(Record("backup"));
        files.files[L"main"] = mainBefore;
        files.files[L"backup"] = backupBefore;
        auto store = MakeStore(files);
        const auto load = store.Load();
        const auto snapshot = store.Snapshot();
        Require(load.mode == CompletionStoreMode::WritableLoaded &&
                    load.failure == CompletionJsonFailure::None &&
                    load.recordCount == 3u &&
                    snapshot.records.size() == 3u &&
                    snapshot.records[1u].stableId ==
                        "map:map\\singleplayer\\thecross.map" &&
                    files.writeCalls == 0u && files.replaceCalls == 0u &&
                    files.files[L"main"] == mainBefore &&
                    files.files[L"backup"] == backupBefore,
                "the reviewed three-record main loads writable without file I/O");
    }
    {
        FakeFileOps files;
        const auto legacy =
            LegacyRecord("antares", LaunchSource::SinglePlayerMap);
        const auto oldBytes = Database(legacy);
        files.files[L"main"] = oldBytes;
        auto store = MakeStore(files);
        const auto load = store.Load();
        const auto snapshot = store.Snapshot();
        Require(load.mode == CompletionStoreMode::WritableLoaded &&
                    files.writeCalls == 1u && files.replaceCalls == 1u &&
                    files.files[L"backup"] == oldBytes &&
                    snapshot.records.size() == 1u &&
                    snapshot.records.front().launchSource ==
                        LaunchSource::SinglePlayerCustomMap &&
                    snapshot.records.front().stableId == legacy.stableId &&
                    snapshot.records.front().completedAtUtc ==
                        legacy.completedAtUtc,
                "writable legacy source is atomically normalized");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = Database(RecordAt(
            "Map\\Multiplayer\\Alien.map", "Alien",
            LaunchSource::LoadSinglePlayerMap));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded &&
                    store.Snapshot().records.front().launchSource ==
                        LaunchSource::SinglePlayerMultiplayerMap,
                "loaded multiplayer-list map is canonically normalized");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = Database(RecordAt(
            "Map\\Campaign\\roman01.map", "roman01",
            LaunchSource::LoadCampaign));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded &&
                    store.Snapshot().records.front().launchSource ==
                        LaunchSource::Campaign,
                "loaded campaign is canonically normalized");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = "{broken";
        const auto legacy =
            LegacyRecord("a", LaunchSource::SinglePlayerMap);
        files.files[L"backup"] = Database(legacy);
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode == CompletionStoreMode::ReadOnlyBackup &&
                    load.recordCount == 1u && files.writeCalls == 0u &&
                    store.Snapshot().records.front().launchSource ==
                        LaunchSource::SinglePlayerMap,
                "corrupt main with valid backup must load read-only");
        files.ResetOperationCounts();
        Require(store.AddIfAbsent(Record("b")).status ==
                    CompletionAddStatus::ReadOnly &&
                    files.writeCalls == 0u,
                "read-only backup mode must refuse all writes");
    }
    {
        FakeFileOps files;
        files.files[L"main"] =
            test_fixtures::UnsupportedVersionDatabase();
        files.files[L"backup"] = Database(Record("backup"));
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode == CompletionStoreMode::ReadOnlyBackup &&
                    load.failure == CompletionJsonFailure::InvalidRecord &&
                    load.recordCount == 1u && files.writeCalls == 0u &&
                    files.replaceCalls == 0u,
                "an unreviewed persistence version still fails closed to backup");
    }
    {
        FakeFileOps files;
        const auto legacy =
            LegacyRecord("failure", LaunchSource::SinglePlayerMap);
        files.files[L"main"] = Database(legacy);
        files.writeFailure =
            CompletionTransactionStage::WriteTemporary;
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode ==
                    CompletionStoreMode::ReadOnlyNormalizationFailed &&
                    store.Snapshot().records.size() == 1u &&
                    store.Snapshot().records.front().launchSource ==
                        LaunchSource::SinglePlayerMap,
                "failed normalization retains original data read-only");
        files.ResetOperationCounts();
        Require(store.AddIfAbsent(Record("later")).status ==
                    CompletionAddStatus::ReadOnly &&
                    files.writeCalls == 0u && files.replaceCalls == 0u,
                "failed normalization disables future writes");
    }
    {
        FakeFileOps files;
        files.files[L"backup"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::ReadOnlyBackup,
                "missing main must not overwrite an existing valid backup");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = "{broken";
        files.files[L"backup"] = "{also-broken";
        auto store = MakeStore(files);
        Require(store.Load().mode ==
                    CompletionStoreMode::ReadOnlyUnavailable &&
                    store.Snapshot().records.empty(),
                "unrecoverable data must not become writable empty data");
    }
    {
        FakeFileOps files;
        files.files[L"temporary"] = Database(Record("stale"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableEmpty &&
                    store.Snapshot().records.empty(),
                "stale temporary data must never be promoted on load");
    }
    {
        FakeFileOps files;
        files.failedRead = L"main";
        auto store = MakeStore(files);
        const auto load = store.Load();
        Require(load.mode == CompletionStoreMode::ReadOnlyUnavailable &&
                    load.error == ERROR_ACCESS_DENIED,
                "main read failure must fail closed with its Win32 error");
    }

    {
        FakeFileOps files;
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableEmpty,
                "first-write fixture must load empty");
        const auto committed = store.AddIfAbsent(Record("a"));
        Require(committed.status == CompletionAddStatus::Committed &&
                    files.moveCalls == 1u &&
                    DecodeCompletionJson(files.files[L"main"]),
                "first write must validate and atomically move main");
        const auto originalBytes = files.files[L"main"];
        files.ResetOperationCounts();
        const auto duplicate = store.AddIfAbsent(Record("a"));
        Require(duplicate.status == CompletionAddStatus::Duplicate &&
                    files.writeCalls == 0u && files.replaceCalls == 0u &&
                    files.moveCalls == 0u &&
                    files.files[L"main"] == originalBytes,
                "duplicate stable ID must perform no file transaction");
    }

    for (const auto stage : {
             CompletionTransactionStage::WriteTemporary,
             CompletionTransactionStage::FlushTemporary,
             CompletionTransactionStage::ReadTemporary,
             CompletionTransactionStage::ValidateTemporary,
             CompletionTransactionStage::BackupMain,
             CompletionTransactionStage::ReplaceMain}) {
        FakeFileOps files;
        files.files[L"main"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded,
                "failure fixture must load valid main");
        const auto oldMain = files.files[L"main"];
        const auto oldSnapshot = store.Snapshot();
        files.ResetOperationCounts();
        if (stage == CompletionTransactionStage::WriteTemporary ||
            stage == CompletionTransactionStage::FlushTemporary) {
            files.writeFailure = stage;
        } else if (stage == CompletionTransactionStage::ReadTemporary) {
            files.failedRead = L"temporary";
        } else if (stage ==
                   CompletionTransactionStage::ValidateTemporary) {
            files.corruptTemporaryRead = true;
        } else {
            files.replaceFailure = stage;
        }
        const auto result = store.AddIfAbsent(Record("b"));
        Require(result.status == CompletionAddStatus::Failed &&
                    result.stage == stage,
                "injected transaction stage must report exact failure");
        Require(files.files[L"main"] == oldMain &&
                    store.Snapshot().records.size() ==
                        oldSnapshot.records.size() &&
                    store.Snapshot().records.front().stableId ==
                        oldSnapshot.records.front().stableId,
                "failed transaction must preserve main and snapshot");
    }

    {
        FakeFileOps files;
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableEmpty,
                "move failure fixture must load empty");
        files.moveFailure = true;
        const auto result = store.AddIfAbsent(Record("a"));
        Require(result.status == CompletionAddStatus::Failed &&
                    result.stage == CompletionTransactionStage::MoveFirst &&
                    files.files.find(L"main") == files.files.end() &&
                    store.Snapshot().records.empty(),
                "failed first move must not publish a database");
    }

    {
        FakeFileOps files;
        files.files[L"main"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded,
                "encode failure fixture must load main");
        auto invalid = Record("b");
        invalid.completedAtUtc = "not-a-time";
        const auto result = store.AddIfAbsent(invalid);
        Require(result.status == CompletionAddStatus::Failed &&
                    result.stage == CompletionTransactionStage::Encode,
                "invalid record must fail before file I/O");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded,
                "manual transaction fixture must load main");
        const auto opened = store.SnapshotWithRevision();
        auto added = Record("b");
        added.recordSource = std::string(kManualCompletionRecordSource);
        ManualCompletionRequest request{};
        request.baselineRevision = opened.revision;
        request.entries = {
            {opened.database.records.front().stableId,
             opened.database.records.front(), false, true},
            {added.stableId, added, true, true},
        };
        files.ResetOperationCounts();
        const auto result = store.ApplyManual(request);
        const auto committed = store.SnapshotWithRevision();
        Require(result.status == ManualCompletionApplyStatus::Committed &&
                    result.revision == opened.revision + 1u &&
                    committed.revision == result.revision &&
                    committed.database.records.size() == 1u &&
                    committed.database.records.front().stableId ==
                        added.stableId &&
                    committed.database.records.front().recordSource ==
                        kManualCompletionRecordSource &&
                    files.writeCalls == 1u && files.replaceCalls == 1u,
                "manual changes must commit as one atomic replacement");

        files.ResetOperationCounts();
        const auto stale = store.ApplyManual(request);
        Require(stale.status == ManualCompletionApplyStatus::Conflict &&
                    stale.revision == committed.revision &&
                    files.writeCalls == 0u &&
                    files.replaceCalls == 0u,
                "stale manager revision must not write");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::WritableLoaded,
                "manual failure fixture must load main");
        const auto opened = store.SnapshotWithRevision();
        ManualCompletionRequest request{};
        request.baselineRevision = opened.revision;
        request.entries = {
            {opened.database.records.front().stableId,
             opened.database.records.front(), false, true},
        };
        files.replaceFailure = CompletionTransactionStage::ReplaceMain;
        const auto result = store.ApplyManual(request);
        const auto retained = store.SnapshotWithRevision();
        Require(result.status == ManualCompletionApplyStatus::Failed &&
                    result.stage == CompletionTransactionStage::ReplaceMain &&
                    retained.revision == opened.revision &&
                    retained.database.records.size() == 1u,
                "failed manual transaction must preserve revision and snapshot");
    }
    {
        FakeFileOps files;
        files.files[L"main"] = "{broken";
        files.files[L"backup"] = Database(Record("a"));
        auto store = MakeStore(files);
        Require(store.Load().mode == CompletionStoreMode::ReadOnlyBackup,
                "manual read-only fixture must load backup");
        const auto opened = store.SnapshotWithRevision();
        ManualCompletionRequest request{};
        request.baselineRevision = opened.revision;
        const auto result = store.ApplyManual(request);
        Require(result.status == ManualCompletionApplyStatus::ReadOnly &&
                    files.writeCalls == 0u,
                "manual manager must not write in backup mode");
    }
    return 0;
}
