# Completion Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Win32 plugin version `0.4.0` so a confirmed native local-player victory is persisted exactly once in a protected JSON database below `Plugins\CampaignCompletion`, while completion-marker rendering remains disabled.

**Architecture:** A game-thread coordinator joins session-bound identity, refined launch policy, and native event `609`, then submits one immutable candidate to a bounded background worker. The worker owns a strict UTF-8 JSON store and commits complete snapshots through injected atomic file operations; runtime wiring derives every private path from the ASI module location.

**Tech Stack:** C++17, Win32 wide-path/file/time APIs, S4ModApi 2.3.2, CMake 3.24+, MSVC Win32 `/W4 /WX /permissive-`, GitHub Actions `windows-2022`, PowerShell packaging and archive verification.

## Global Constraints

- Work directly on `main`; the user explicitly rejected worktree isolation.
- Do not spawn subagents; the active developer instruction forbids delegation unless the user explicitly requests it.
- Follow strict TDD for every production behavior: commit and push the failing test, observe the expected GitHub Actions failure, then implement and observe a green run.
- The game must remain closed until a path-aware `0.4.0` build is deployed. Do not start or terminate the game.
- Do not deploy during this plan. A separately approved deployment is required before modifying the authorized Settlers United archive or live plugin files.
- Do not modify or delete any game/SU file except the separately authorized `CampaignCompletion*.asi`, project-owned `CampaignCompletion` directory, and guarded exact replacement of `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
- The live project-owned directory is `<game>\Plugins\CampaignCompletion`; never recreate `<game>\CampaignCompletion`.
- `completed_maps.json` is schema version `1`, UTF-8, at most `4 MiB`, at most `8192` records, and at most `1024` UTF-16 code units per decoded string.
- Completion queue capacity is exactly `32`; controlled drain timeout is exactly `5000 ms`.
- Duplicate victories preserve the first record byte-for-byte and do not write a transaction.
- Random maps are recordable as `map_kind=random`; future marker visibility remains false.
- Corrupt main plus valid backup loads the backup read-only. No automatic recovery, promotion, deletion, or overwrite is permitted.
- `CompletionDetection=1`, `CompletionStorage=1`, and `CompletionMarkers=0` in version `0.4.0`.
- Do not add process patches, Lua writes, game-data writes, `GameDefaultGameEndCheck`, victory-screen recognition, or any new hook.

## CI RED/GREEN protocol

The local machine has no installed MSVC Win32 toolchain, so every compile/test gate runs in GitHub Actions.

For a RED commit:

```bash
git push origin main
sha="$(git rev-parse HEAD)"
run_id="$(gh run list --workflow build-debug-asi.yml --branch main \
  --commit "$sha" --json databaseId --jq '.[0].databaseId')"
gh run watch "$run_id" --exit-status
```

Expected: nonzero status in Build because the new test references an absent declaration, or in Test because the new behavioral assertion fails for the intended missing behavior. Inspect the failed job with:

```bash
gh run view "$run_id" --log-failed
```

For a GREEN commit, repeat the same four commands. Expected: workflow conclusion `success`, including archive integration, Win32 configure/build, policy verification, tests, packaging, PE32/layout verification, and artifact upload.

---

### Task 1: Module-relative private paths and package layout

**Files:**
- Create: `src/runtime/PluginPaths.h`
- Create: `src/runtime/PluginPaths.cpp`
- Create: `tests/PluginPathsTests.cpp`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `tests/TestMain.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tools/package_debug.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Produces `PluginPaths BuildPluginPaths(const std::filesystem::path& modulePath)`.
- Produces `std::optional<PluginPaths> ResolvePluginPaths(HMODULE module) noexcept`.
- `PluginPaths` contains `module`, `pluginDirectory`, `dataDirectory`, `ini`, `log`, `stop`, `database`, `temporary`, and `backup`.
- Later tasks consume `PluginPaths::database`, `temporary`, and `backup`.

- [ ] **Step 1: Add failing path and package-policy tests**

Add `RunPluginPathsTests()` and require these exact relationships:

```cpp
const auto paths = BuildPluginPaths(
    LR"(F:\Games\thesettlers4\Plugins\CampaignCompletionDebug.asi)");
Require(paths.pluginDirectory == LR"(F:\Games\thesettlers4\Plugins)",
        "plugin directory must be the ASI parent");
Require(paths.dataDirectory ==
            LR"(F:\Games\thesettlers4\Plugins\CampaignCompletion)",
        "data directory must be below Plugins");
Require(paths.ini == paths.dataDirectory / L"CampaignCompletionDebug.ini",
        "INI must be module-relative");
Require(paths.database == paths.dataDirectory / L"completed_maps.json",
        "database must be module-relative");
```

Update `RuntimePolicyTests.cpp` to reject the old `pluginDirectory.parent_path()` data-root derivation and require the packaged path `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`.

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/PluginPathsTests.cpp \
  tests/RuntimePolicyTests.cpp
git commit -m "test: require plugin-relative completion paths"
```

Run the CI RED protocol. Expected build failure: `runtime/PluginPaths.h` is missing.

- [ ] **Step 3: Implement the path boundary and switch runtime/package paths**

Create this public shape:

```cpp
struct PluginPaths final {
    std::filesystem::path module;
    std::filesystem::path pluginDirectory;
    std::filesystem::path dataDirectory;
    std::filesystem::path ini;
    std::filesystem::path log;
    std::filesystem::path stop;
    std::filesystem::path database;
    std::filesystem::path temporary;
    std::filesystem::path backup;
};

PluginPaths BuildPluginPaths(const std::filesystem::path& modulePath);
std::optional<PluginPaths> ResolvePluginPaths(HMODULE module) noexcept;
```

`BuildPluginPaths` applies `lexically_normal()` without filesystem probing,
takes exactly one parent as the plugin directory, and appends
`CampaignCompletion`. `ResolvePluginPaths` uses the existing bounded
`GetModuleFileNameW` pattern and returns `nullopt` on truncation or exception.

Replace every root-level path derivation in `DiagnosticRuntime::Start` with the returned structure. Package the INI under `staging/Plugins/CampaignCompletion`, and change the workflow's exact expected archive layout to:

```text
Plugins/CampaignCompletion/CampaignCompletionDebug.ini
Plugins/CampaignCompletionDebug.asi
```

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/runtime/PluginPaths.* src/runtime/DiagnosticRuntime.* \
  tests/TestMain.cpp tests/PluginPathsTests.cpp tests/RuntimePolicyTests.cpp \
  tools/package_debug.ps1 .github/workflows/build-debug-asi.yml
git commit -m "feat: resolve data below plugin directory"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 2: Immutable completion record and stable identity

**Files:**
- Create: `src/completion/CompletionRecord.h`
- Create: `src/completion/CompletionRecord.cpp`
- Create: `tests/CompletionRecordTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `CompletionRecord`, `CompletionMapKind`, and constants for schema/plugin/game versions.
- Produces `std::optional<std::string> BuildStableMapId(std::wstring_view relative)`.
- Produces `std::optional<std::string> WideToStrictUtf8(std::wstring_view value)`.
- Produces `std::optional<std::string> FormatUtcCompletionTime(const SYSTEMTIME& utc)` and `bool IsValidUtcCompletionTime(std::string_view value)`.
- Produces `std::string CurrentUtcCompletionTime() noexcept`, returning an
  empty string only if Win32 time formatting fails.
- Produces `CompletionMapKind CompletionKindFor(std::wstring_view relative) noexcept`.

- [ ] **Step 1: Add failing record tests**

Test exact canonicalization and time rules:

```cpp
Require(BuildStableMapId(L"Map/User/Battle of the Gods.map") ==
            std::optional<std::string>(
                "map:map\\user\\battle of the gods.map"),
        "stable ID must normalize separator and case");
Require(BuildStableMapId(L"MAP\\USER\\BATTLE OF THE GODS.MAP") ==
            BuildStableMapId(L"Map/User/Battle of the Gods.map"),
        "stable ID must be case-insensitive");
Require(!BuildStableMapId(L"C:\\absolute.map"),
        "absolute identity must be rejected");
Require(!BuildStableMapId(L"Map\\..\\escape.map"),
        "parent traversal must be rejected");
Require(CompletionKindFor(L"RD_LCGSDR30") == CompletionMapKind::Random,
        "RD_ identifier must remain recordable random");

SYSTEMTIME utc{2026, 7, 2, 14, 1, 57, 38, 0};
Require(FormatUtcCompletionTime(utc) ==
            std::optional<std::string>("2026-07-14T01:57:38Z"),
        "UTC time must use whole-second ISO 8601");
Require(IsValidUtcCompletionTime("2024-02-29T23:59:59Z"),
        "valid leap day must pass");
Require(!IsValidUtcCompletionTime("2026-02-29T00:00:00Z"),
        "invalid calendar date must fail");
```

Also test empty input, embedded NUL, 513-code-unit relative input, invalid surrogate pairs, `[random]`, `<random>`, non-random lowercase `rd_`, UTF-8 non-ASCII display text, and a generated ID over 1024 units.

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionRecordTests.cpp
git commit -m "test: define completion record identity"
```

Run the CI RED protocol. Expected build failure: `completion/CompletionRecord.h` is missing.

- [ ] **Step 3: Implement immutable record primitives**

Use this record shape:

```cpp
enum class CompletionMapKind { Fixed, Random };

struct CompletionRecord final {
    std::string stableId;
    std::string relativeId;
    std::string displayName;
    CompletionMapKind mapKind = CompletionMapKind::Fixed;
    LaunchSource launchSource = LaunchSource::Unknown;
    std::string completedAtUtc;
    std::string recordSource = "native-event-609";
    std::string gameVersion = "2.50.1516.0";
    std::string pluginVersion = "0.4.0";
};
```

Implement strict UTF-16 validation before `WideCharToMultiByte(CP_UTF8,
WC_ERR_INVALID_CHARS, ...)`. Normalize `/` to `\`, reject drive/UNC/rooted and
`.`/`..` components, lowercase with `LCMapStringEx(LOCALE_NAME_INVARIANT,
LCMAP_LOWERCASE, ...)`, then UTF-8 encode and prefix `map:`. Validate UTC text
by exact length/separators, decimal fields, `SystemTimeToFileTime`, and
round-trip `FileTimeToSystemTime` equality. `CurrentUtcCompletionTime` calls
`GetSystemTime`, formats it through the same function, and returns the value or
an empty string.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionRecord.* \
  tests/TestMain.cpp tests/CompletionRecordTests.cpp
git commit -m "feat: add immutable completion records"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 3: Session-bound completion candidate coordination

**Files:**
- Create: `src/completion/CompletionCandidateCoordinator.h`
- Create: `src/completion/CompletionCandidateCoordinator.cpp`
- Create: `tests/CompletionCandidateCoordinatorTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `ConfirmedMapIdentity`, `LaunchOriginSnapshot`, `VictoryEventSnapshot`, `BuildStableMapId`, and `ShouldRecordVictory`.
- Produces `CompletionCandidate` and one candidate-returning observation API.

- [ ] **Step 1: Add failing session-gate tests**

Use an injected deterministic clock and test both arrival orders:

```cpp
CompletionCandidateCoordinator coordinator(
    [] { return std::string("2026-07-14T01:57:38Z"); });
coordinator.BeginSession(4u, {LaunchSource::LoadMapUnresolved,
                              SessionEligibility::Unknown});
Require(!coordinator.ObserveVictory(Won(4u)),
        "victory must wait for identity and refined policy");
const auto candidate = coordinator.ObserveIdentity(
    ConfirmedMapIdentity{4u, L"Battle of the Gods",
                         L"Map\\User\\Battle of the Gods.map"},
    {LaunchSource::LoadSinglePlayerMap, SessionEligibility::Eligible});
Require(candidate && candidate->record.stableId ==
            "map:map\\user\\battle of the gods.map",
        "matching loaded session must emit once");
Require(!coordinator.ObserveVictory(Won(4u)),
        "duplicate victory must not emit twice");
```

Add tests for identity-first, mismatched identity/victory sessions, session zero,
new MapInit reset, lost, malformed, conflict, orphan, unknown eligibility,
online multiplayer, fixed direct launch, and random-map eligibility.

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp \
  tests/CompletionCandidateCoordinatorTests.cpp
git commit -m "test: require session-bound completion candidates"
```

Run the CI RED protocol. Expected build failure: the coordinator header is missing.

- [ ] **Step 3: Implement the coordinator state machine**

Expose:

```cpp
struct CompletionCandidate final {
    std::uint64_t sessionId = 0;
    CompletionRecord record;
};

class CompletionCandidateCoordinator final {
public:
    using Clock = std::function<std::string()>;
    explicit CompletionCandidateCoordinator(Clock clock);
    void BeginSession(std::uint64_t sessionId,
                      LaunchOriginSnapshot origin) noexcept;
    std::optional<CompletionCandidate> ObserveIdentity(
        const ConfirmedMapIdentity& identity,
        LaunchOriginSnapshot refinedOrigin) noexcept;
    std::optional<CompletionCandidate> ObserveVictory(
        const VictoryEventSnapshot& victory) noexcept;
    void Disable() noexcept;
};
```

Retain at most one identity and one winning snapshot for the active session.
`TryComplete()` requires exact session equality, `Won`, successful stable-ID and
strict UTF-8 conversions, valid injected UTC text, and eligible origin. Set
`emitted_` before returning so callbacks cannot duplicate the candidate.
Catch allocation/conversion/clock exceptions and return `nullopt`.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionCandidateCoordinator.* \
  tests/TestMain.cpp tests/CompletionCandidateCoordinatorTests.cpp
git commit -m "feat: coordinate session-bound completion candidates"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 4: Strict bounded JSON codec

**Files:**
- Create: `src/completion/CompletionJson.h`
- Create: `src/completion/CompletionJson.cpp`
- Create: `tests/CompletionJsonTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `CompletionRecord`.
- Produces `CompletionDatabaseSnapshot`, `CompletionJsonFailure`, `EncodeCompletionJson`, and `DecodeCompletionJson`.

- [ ] **Step 1: Add failing codec tests**

Create a two-record snapshot in reverse stable-ID order and require encoded
output to be deterministically sorted and newline terminated. Decode it and
compare all fields. Add one focused rejection assertion for every specified
failure: over `4 MiB`, more than `8192` records, string over `1024` decoded
units, illegal UTF-8, bad escape, lone high/low surrogate, duplicate key,
unknown top-level key, unknown record key, missing key, wrong type, schema not
`1`, duplicate stable ID, inconsistent stable ID/relative ID, bad enum, bad
timestamp, trailing bytes, and nesting deeper than the exact schema.

The primary round-trip assertion is:

```cpp
const auto encoded = EncodeCompletionJson({second, first});
Require(encoded && encoded->back() == '\n',
        "encoded JSON must end with one newline");
const auto decoded = DecodeCompletionJson(*encoded);
Require(decoded && decoded.snapshot.records.size() == 2u,
        "valid JSON must round-trip");
Require(decoded.snapshot.records[0].stableId <
            decoded.snapshot.records[1].stableId,
        "records must be sorted by stable ID");
```

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionJsonTests.cpp
git commit -m "test: define strict completion JSON schema"
```

Run the CI RED protocol. Expected build failure: `completion/CompletionJson.h` is missing.

- [ ] **Step 3: Implement the schema-specific parser and encoder**

Expose:

```cpp
inline constexpr std::size_t kMaximumCompletionJsonBytes = 4u * 1024u * 1024u;
inline constexpr std::size_t kMaximumCompletionRecords = 8192u;
inline constexpr std::size_t kMaximumCompletionStringUnits = 1024u;

struct CompletionDatabaseSnapshot final {
    std::vector<CompletionRecord> records;
};

enum class CompletionJsonFailure {
    None, Oversized, Syntax, Utf8, DuplicateKey, UnknownField,
    MissingField, WrongType, Schema, FieldLimit, InvalidRecord,
    DuplicateStableId
};

struct CompletionJsonResult final {
    CompletionDatabaseSnapshot snapshot;
    CompletionJsonFailure failure = CompletionJsonFailure::None;
    explicit operator bool() const noexcept {
        return failure == CompletionJsonFailure::None;
    }
};

std::optional<std::string> EncodeCompletionJson(
    CompletionDatabaseSnapshot snapshot) noexcept;
CompletionJsonResult DecodeCompletionJson(std::string_view bytes) noexcept;
```

Implement a cursor parser for only the approved grammar: top-level object with
integer `schema_version` and array `records`; each record object has exactly
the nine required string fields. Parse arbitrary field order while tracking a
bitmask to reject duplicate/unknown/missing keys. Decode raw UTF-8 strictly and
JSON escapes including paired `\uD800`–`\uDFFF`; reject control bytes and lone
surrogates. Validate every record through Task 2 functions, exact enum/name
tables, and recomputed stable ID. Accept only recordable launch sources:
`single-player-map`, `single-player-multiplayer-map`,
`single-player-custom-map`, `campaign`, `random-map`, `load-campaign`, and
`load-single-player-map`; reject unknown, unresolved, and online source names.
Sort and reject adjacent duplicate IDs.
The encoder escapes quotes, backslashes, controls, emits valid UTF-8 unchanged,
uses a fixed field order, sorts records, and returns `nullopt` if its own output
fails size bounds.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionJson.* \
  tests/TestMain.cpp tests/CompletionJsonTests.cpp
git commit -m "feat: add strict bounded completion JSON"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 5: Transactional completion store and Win32 file boundary

**Files:**
- Create: `src/completion/CompletionFileOps.h`
- Create: `src/completion/Win32CompletionFileOps.h`
- Create: `src/completion/Win32CompletionFileOps.cpp`
- Create: `src/completion/CompletionStore.h`
- Create: `src/completion/CompletionStore.cpp`
- Create: `tests/CompletionStoreTests.cpp`
- Create: `tests/Win32CompletionFileOpsTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `PluginPaths`, `CompletionRecord`, and the JSON codec.
- Produces `ICompletionFileOps`, `Win32CompletionFileOps`, `CompletionStore`, load modes, transaction stages, and immutable snapshots.

- [ ] **Step 1: Add failing store and real-file tests**

Implement a fake file boundary that records operations and can fail exactly
`ReadMain`, `ReadBackup`, `WriteTemporary`, `FlushTemporary`,
`ReadTemporary`, `ReplaceMain`, or `MoveFirst`. Require these load modes:

```cpp
Require(LoadWith({}, {}).mode == CompletionStoreMode::WritableEmpty,
        "no main and no backup must be writable empty");
Require(LoadWith(validMain, invalidBackup).mode ==
            CompletionStoreMode::WritableLoaded,
        "valid main must remain writable");
Require(LoadWith(invalidMain, validBackup).mode ==
            CompletionStoreMode::ReadOnlyBackup,
        "valid backup must load read-only");
Require(LoadWith(missingMain, validBackup).mode ==
            CompletionStoreMode::ReadOnlyBackup,
        "missing main must not overwrite an existing backup");
Require(LoadWith(invalidMain, invalidBackup).mode ==
            CompletionStoreMode::ReadOnlyUnavailable,
        "unrecoverable data must not become empty writable data");
```

For every injected transaction failure, assert old main bytes and published
snapshot are unchanged. Require duplicate insertion to perform zero file
operations. In a unique project-temporary directory, test the real Win32
boundary creates/flushed/moves a first file, then replaces it while producing
a backup containing the old bytes. Never point tests at the game directory.

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionStoreTests.cpp \
  tests/Win32CompletionFileOpsTests.cpp
git commit -m "test: define transactional completion storage"
```

Run the CI RED protocol. Expected build failure: completion-store headers are missing.

- [ ] **Step 3: Implement injected transactions and Win32 operations**

Use this boundary:

```cpp
enum class CompletionFileStatus { Success, Missing, Failed };
struct CompletionReadResult final {
    CompletionFileStatus status = CompletionFileStatus::Failed;
    std::string bytes;
    DWORD error = ERROR_SUCCESS;
};
enum class CompletionTransactionStage {
    None, ReadMain, ReadBackup, Encode, WriteTemporary, FlushTemporary,
    ReadTemporary, ValidateTemporary, BackupMain, ReplaceMain, MoveFirst
};
struct CompletionWriteResult final {
    bool success = false;
    CompletionTransactionStage stage = CompletionTransactionStage::None;
    DWORD error = ERROR_SUCCESS;
};

class ICompletionFileOps {
public:
    virtual ~ICompletionFileOps() = default;
    virtual CompletionReadResult ReadBounded(
        const std::filesystem::path& path, std::size_t limit) noexcept = 0;
    virtual CompletionWriteResult WriteTemporaryAndFlush(
        const std::filesystem::path& path, std::string_view bytes) noexcept = 0;
    virtual CompletionWriteResult ReplaceWithBackup(
        const std::filesystem::path& main,
        const std::filesystem::path& temporary,
        const std::filesystem::path& backup) noexcept = 0;
    virtual CompletionWriteResult MoveFirstWriteThrough(
        const std::filesystem::path& temporary,
        const std::filesystem::path& main) noexcept = 0;
};
```

`WriteTemporaryAndFlush` uses `CreateFileW(CREATE_ALWAYS)`, a complete bounded
`WriteFile` loop, and `FlushFileBuffers`. `ReplaceWithBackup` uses
`ReplaceFileW`; `MoveFirstWriteThrough` uses `MoveFileExW` with
`MOVEFILE_WRITE_THROUGH` and without replace-existing. Preserve `GetLastError`
at each failing API. The combined write method reports `WriteTemporary` for
create/write failure and `FlushTemporary` for flush failure. The fake boundary
may report `BackupMain` before atomic replacement or `ReplaceMain` for the
replacement call, allowing both designed failure stages to be verified even
though production `ReplaceFileW` performs them atomically.

Define the store contract exactly as:

```cpp
enum class CompletionStoreMode {
    Uninitialized, WritableEmpty, WritableLoaded,
    ReadOnlyBackup, ReadOnlyUnavailable
};
enum class CompletionAddStatus {
    Committed, Duplicate, ReadOnly, Failed
};
struct CompletionLoadResult final {
    CompletionStoreMode mode = CompletionStoreMode::Uninitialized;
    CompletionJsonFailure failure = CompletionJsonFailure::None;
    DWORD error = ERROR_SUCCESS;
    std::size_t recordCount = 0u;
};
struct CompletionAddResult final {
    CompletionAddStatus status = CompletionAddStatus::Failed;
    CompletionTransactionStage stage = CompletionTransactionStage::None;
    DWORD error = ERROR_SUCCESS;
};
class ICompletionStore {
public:
    virtual ~ICompletionStore() = default;
    virtual CompletionAddResult AddIfAbsent(
        const CompletionRecord& record) noexcept = 0;
};
class CompletionStore final : public ICompletionStore {
public:
    CompletionStore(ICompletionFileOps& files,
                    std::filesystem::path main,
                    std::filesystem::path temporary,
                    std::filesystem::path backup);
    CompletionLoadResult Load() noexcept;
    CompletionAddResult AddIfAbsent(
        const CompletionRecord& record) noexcept override;
    CompletionStoreMode Mode() const noexcept;
    CompletionDatabaseSnapshot Snapshot() const;
};
```

Load main and backup according to the four exact
modes. A transaction serializes a copied snapshot, writes/flushed temp,
re-reads and decodes temp, then chooses replace or first move. Publish only
after success. Protect mode/snapshot/index publication and queries with a
mutex. Return structured status, failing stage, and Win32 error.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionFileOps.h \
  src/completion/Win32CompletionFileOps.* src/completion/CompletionStore.* \
  tests/TestMain.cpp tests/CompletionStoreTests.cpp \
  tests/Win32CompletionFileOpsTests.cpp
git commit -m "feat: add transactional completion store"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 6: Fixed-capacity persistence worker

**Files:**
- Create: `src/completion/CompletionWorker.h`
- Create: `src/completion/CompletionWorker.cpp`
- Create: `tests/CompletionWorkerTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `CompletionCandidate`, `ICompletionStore`, and a log sink.
- Produces nonblocking `TryEnqueue`, `Start`, and bounded `StopAndDrain`.

- [ ] **Step 1: Add failing worker tests**

Use a controllable fake store to block processing. Require exactly 32 queued
candidates are accepted and the 33rd is rejected without waiting. Release the
store and require FIFO processing. Require duplicate store results produce no
error, transaction failures log stable ID/stage/error, exceptions are caught,
new admissions stop immediately after shutdown begins, a normal drain returns
true, and a blocked drain returns false within a measured bound without
destroying worker state.

```cpp
for (std::uint64_t id = 1; id <= 32; ++id) {
    Require(worker.TryEnqueue(Candidate(id)), "first 32 entries must fit");
}
Require(!worker.TryEnqueue(Candidate(33)),
        "33rd entry must be rejected by the nonblocking bound");
```

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionWorkerTests.cpp
git commit -m "test: require bounded completion worker"
```

Run the CI RED protocol. Expected build failure: `completion/CompletionWorker.h` is missing.

- [ ] **Step 3: Implement the worker lifecycle**

Expose:

```cpp
class CompletionWorker final {
public:
    using LogSink = std::function<void(LogLevel, std::string)>;
    CompletionWorker(ICompletionStore& store, LogSink sink);
    ~CompletionWorker();
    bool Start() noexcept;
    bool TryEnqueue(CompletionCandidate candidate) noexcept;
    bool StopAndDrain(std::chrono::milliseconds timeout) noexcept;
    bool Running() const noexcept;
private:
    static constexpr std::size_t kCapacity = 32u;
};
```

Protect the deque and lifecycle flags with one mutex and condition variables.
The consumer waits for work or stop, moves one candidate at a time, and calls
`AddIfAbsent`. Catch all exceptions at both public and thread boundaries.
Count the currently processed candidate as outstanding until its store call
returns, so dequeueing cannot make a 33rd admission fit while 32 operations are
still pending.
`StopAndDrain` closes admission, signals the consumer, waits up to the supplied
timeout for queue-empty/thread-finished, joins only after finished, and returns
false without detaching or destroying state on timeout.
The destructor closes admission, signals the consumer, and joins without
detaching; runtime integration therefore guarantees that live process teardown
never invokes this destructor under the loader lock.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionWorker.* \
  tests/TestMain.cpp tests/CompletionWorkerTests.cpp
git commit -m "feat: add bounded completion worker"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 7: Runtime integration, shutdown safety, and version policy

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Create: `src/completion/CompletionAdmission.h`
- Create: `src/completion/CompletionAdmission.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `tests/RuntimePolicyTests.cpp`
- Create: `tests/CompletionRuntimeFlowTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tools/verify_no_process_patch.ps1`

**Interfaces:**
- Produces `CompletionAdmission`, the exact integration seam between listener
  observations and the worker enqueue sink.
- `S4Listeners::Start` additionally consumes `CompletionAdmission&`.
- `DiagnosticRuntime` owns file ops, store, worker, and candidate coordinator in dependency-safe declaration order.
- Runtime version becomes `0.4.0` and mode becomes `completion-persistence`.

- [ ] **Step 1: Add failing integration and policy tests**

Instantiate `CompletionAdmission` with a deterministic coordinator and a fake
submit sink, and prove:

```text
MapInit session 4
-> confirmed Battle of the Gods identity for session 4
-> refined load-single-player-map eligible origin
-> native event 609 won for session 4
-> exactly one worker enqueue
```

Repeat with victory before identity. Prove session-3 identity plus session-4
victory never enqueues; lost/online never enqueue; random enqueues with
`map_kind=random`; a new MapInit clears pending state.

Update runtime policy assertions to require version/mode/path and exactly:

```text
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=0
HookCount=0
CodePatchBytes=0
LuaWrites=0
GameDataWrites=0
```

Require `StopAndDrain(std::chrono::milliseconds(5000))`, and require runtime
resources remain owned when drain reports false. Require `RuntimeInstance()`
to use the process-lifetime pointer shown below, and require `DllMain` to have
no `DLL_PROCESS_DETACH` branch or completion-worker cleanup.

- [ ] **Step 2: Commit, push, and verify RED**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionRuntimeFlowTests.cpp \
  tests/RuntimePolicyTests.cpp
git commit -m "test: require completion persistence runtime flow"
```

Run the CI RED protocol. Expected build/test failure because runtime has no completion coordinator/worker integration and policy remains `0.3.3` disabled.

- [ ] **Step 3: Wire completion admission and storage**

At runtime startup:

```cpp
paths_ = ResolvePluginPaths(module);
logger_.Open(paths_->log);
fileOps_ = std::make_unique<Win32CompletionFileOps>();
store_ = std::make_unique<CompletionStore>(
    *fileOps_, paths_->database, paths_->temporary, paths_->backup);
const auto load = store_->Load();
worker_ = std::make_unique<CompletionWorker>(*store_, logSink);
worker_->Start();
candidateCoordinator_ = std::make_unique<CompletionCandidateCoordinator>(
    [] { return CurrentUtcCompletionTime(); });
admission_ = std::make_unique<CompletionAdmission>(
    *candidateCoordinator_,
    [this](CompletionCandidate candidate) {
        return worker_ != nullptr && worker_->TryEnqueue(std::move(candidate));
    });
```

Log the store mode and record count without absolute save/user paths. A valid
read-only mode is allowed to start; worker additions return read-only refusal.
Failure to resolve paths, open the logger, construct/load the store, or start
the worker cleanly aborts plugin initialization without propagating into the
game.

Create the exact seam:

```cpp
class CompletionAdmission final {
public:
    using Submit = std::function<bool(CompletionCandidate)>;
    CompletionAdmission(CompletionCandidateCoordinator& coordinator,
                        Submit submit) noexcept;
    void BeginSession(std::uint64_t sessionId,
                      LaunchOriginSnapshot origin) noexcept;
    bool ObserveIdentity(const ConfirmedMapIdentity& identity,
                         LaunchOriginSnapshot refinedOrigin) noexcept;
    bool ObserveVictory(const VictoryEventSnapshot& victory) noexcept;
    void Disable() noexcept;
};
```

Both observation methods call the coordinator and, only when it returns a
candidate, invoke the same nonblocking submit function exactly once. Catch
submit exceptions and return false. On MapInit, `S4Listeners` calls
`admission_->BeginSession(activeSessionId_, activeOrigin_)`. After identity
refinement it calls `ObserveIdentity`; after consuming/logging the native event
it calls `ObserveVictory`. The runtime submit lambda calls
`worker_->TryEnqueue` and logs admission or rejection.

During controlled stop, first disable candidate admission, detach native
subscription, and stop listeners. Then call
`StopAndDrain(std::chrono::milliseconds(5000))`. On timeout write the approved
failure trace and retain worker/store/logger/runtime ownership; on success
close resources in reverse dependency order. Update configuration, bootstrap
version/mode, CMake source lists, and policy verification. Add the new
completion sources to the production target without adding any hook source.

Change `RuntimeInstance()` to return a process-lifetime heap allocation that is
never destroyed automatically:

```cpp
DiagnosticRuntime& RuntimeInstance() {
    static DiagnosticRuntime* const runtime = new DiagnosticRuntime();
    return *runtime;
}
```

The operating system reclaims it at process exit. Controlled stop still closes
all owned resources explicitly. This prevents C++ thread, mutex, stream, or
filesystem destructors from running inside DLL process-detach loader lock; do
not add any `DLL_PROCESS_DETACH` cleanup to `DllMain`.

- [ ] **Step 4: Commit, push, and verify GREEN**

```bash
git add CMakeLists.txt src/completion/CompletionAdmission.* \
  src/runtime/S4Listeners.* \
  src/runtime/DiagnosticRuntime.* config/CampaignCompletionDebug.ini \
  tests/TestMain.cpp tests/CompletionRuntimeFlowTests.cpp \
  tests/RuntimePolicyTests.cpp tools/verify_no_process_patch.ps1
git commit -m "feat: persist confirmed completion victories"
```

Run the CI GREEN protocol and require workflow success.

---

### Task 8: Full verification, artifact inspection, and deployment handoff

**Files:**
- Create: `docs/research/phase-4-completion-persistence-build-report.md`
- Generate but do not commit: `artifacts/phase-4-completion-persistence-ci/<run-id>/...`

**Interfaces:**
- Consumes the final successful GitHub artifact.
- Produces a reviewable build report and exact hashes; performs no deployment.

- [ ] **Step 1: Run the final full workflow from a clean tracked tree**

```bash
git diff --check
git status --short
git push origin main
sha="$(git rev-parse HEAD)"
run_id="$(gh run list --workflow build-debug-asi.yml --branch main \
  --commit "$sha" --json databaseId --jq '.[0].databaseId')"
gh run watch "$run_id" --exit-status
gh run view "$run_id" --json conclusion,jobs,headSha,url
```

Expected: success, zero tracked changes, with only the already intentional
untracked `artifacts/` and save-sample evidence directories.

- [ ] **Step 2: Download and independently inspect the artifact**

```bash
mkdir -p "artifacts/phase-4-completion-persistence-ci/$run_id"
gh run download "$run_id" --name CampaignCompletionDebug-Win32 \
  --dir "artifacts/phase-4-completion-persistence-ci/$run_id"
sha256sum "artifacts/phase-4-completion-persistence-ci/$run_id"/*
```

Expand the outer artifact and inner package into project-owned artifact
directories. Require exactly:

```text
Plugins/CampaignCompletion/CampaignCompletionDebug.ini
Plugins/CampaignCompletionDebug.asi
```

Verify the ASI is PE32/i386, policy verifier passes, packaged INI has version
`0.4.0`, detection/storage enabled, markers disabled, and packaged trace root
empty. Run `tools/test_settlers_united_artifact.ps1` again against temporary
archives.

- [ ] **Step 3: Review scope and write the build report**

Review every changed completion/runtime/tool file for session mixing, callback
I/O, unbounded allocation, unsafe shutdown, accidental absolute paths, parser
acceptance gaps, process patches, and unauthorized game writes. Record commit,
run/job/artifact IDs, artifact/inner-package/ASI/INI SHA-256 values, exact ZIP
layout, PE machine, test counts, policy values, and the explicit statement:

```text
No deployment was performed. The game must remain closed until the user
approves guarded deployment of this verified 0.4.0 artifact.
```

- [ ] **Step 4: Commit and push the report**

```bash
git add docs/research/phase-4-completion-persistence-build-report.md
git commit -m "docs: verify completion persistence build"
git push origin main
```

Do not deploy. Ask the user for explicit deployment approval and include the
verified hashes and the reminder that the current live `0.3.3` binary cannot
use the migrated configuration path.
