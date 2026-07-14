# Map Category Normalization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make direct launches and loaded saves of the same map persist the same canonical map category, while preserving online exclusion and safely normalizing existing schema-version-1 records.

**Architecture:** Keep navigation and load controls as the admission boundary, then canonicalize eligible sessions from the validated SU relative identity in `MapSessionPolicy`. Reuse that pure classifier in `CompletionStore` to normalize legacy metadata through the existing verified atomic transaction; a failed normalization loads the original snapshot read-only instead of publishing partial state.

**Tech Stack:** C++17, Win32 UTF conversion and file replacement, S4ModApi listener surface, CMake/CTest, MSVC Win32 `/W4 /WX`, PowerShell packaging and policy checks, GitHub Actions.

## Global Constraints

- Never parse or modify a `.sav` file; classification uses only validated `SU.Game.GetMapNameRelativePath()` output.
- Never add a game hook, process-memory probe, Lua write, or game-directory write.
- Online-multiplayer exclusion is decided before path classification and always wins.
- `Map\Singleplayer`, `Map\Multiplayer`, and `Map\User` comparisons are case-insensitive, slash-insensitive, and require a complete directory prefix.
- Unknown but valid fixed-map paths use `single-player-map`.
- Random victories remain recordable and their future markers remain hidden.
- Schema version remains `1`; legacy sources remain readable, while new writes are canonical.
- Normalization may change only `launch_source`; stable identity, first completion time, record count, ordering, and every other field remain unchanged.
- Read-only backup recovery performs no normalization write.
- Deployment is forbidden while `S4_Main.exe` is running and must use the existing guarded archive procedure after the user closes the game.
- Work directly on `main`, as explicitly requested; do not create a worktree or feature branch.

---

## File structure

- `src/victory/MapSessionPolicy.h`: public pure canonical-category interface and existing admission/refinement policy.
- `src/victory/MapSessionPolicy.cpp`: component-aware path comparison and canonical category selection.
- `tests/MapSessionPolicyTests.cpp`: direct/load equivalence, boundary, online, campaign, random, and fallback tests.
- `src/completion/CompletionRecord.h`: shared strict UTF-8-to-UTF-16 conversion declaration.
- `src/completion/CompletionRecord.cpp`: shared strict conversion implementation.
- `src/completion/CompletionJson.cpp`: consume the shared conversion instead of a private duplicate.
- `src/completion/CompletionStore.h`: normalization-failure mode and shared transaction helper declaration.
- `src/completion/CompletionStore.cpp`: writable-main normalization and shared verified commit path.
- `tests/CompletionRecordTests.cpp`: strict conversion regression coverage after moving the helper.
- `tests/CompletionStoreTests.cpp`: normalization, no-op, failure, and read-only recovery tests.
- `src/runtime/DiagnosticRuntime.cpp`: readable name for the new read-only mode.
- `tests/CompletionCandidateCoordinatorTests.cpp`: canonical loaded/direct candidate expectations.
- `tests/CompletionRuntimeFlowTests.cpp`: end-to-end admission seam proves direct/load source equality.
- `docs/research/phase-4-completion-persistence-build-report.md`: final CI artifact hashes and verification evidence.

---

### Task 1: Canonical identity classifier and session refinement

**Files:**
- Modify: `src/victory/MapSessionPolicy.h`
- Modify: `src/victory/MapSessionPolicy.cpp`
- Test: `tests/MapSessionPolicyTests.cpp`

**Interfaces:**
- Consumes: validated SU relative identity and an admitted `LaunchOriginSnapshot`.
- Produces: `LaunchSource CanonicalCompletionSource(LaunchSource admittedSource, std::wstring_view relativeIdentifier) noexcept` and canonical output from the existing `RefineLaunchOrigin`/`RefineActiveSessionOrigin` functions.

- [ ] **Step 1: Add failing direct/load equivalence and boundary tests**

Extend `RunMapSessionPolicyTests()` with a table covering the three fixed-map directories and both entry routes:

```cpp
struct CategoryCase final {
    std::wstring_view relative;
    LaunchSource expected;
};
for (const auto& value : {
         CategoryCase{L"Map\\Singleplayer\\Aeneas.map",
                      LaunchSource::SinglePlayerMap},
         CategoryCase{L"map/multiplayer/Alien.map",
                      LaunchSource::SinglePlayerMultiplayerMap},
         CategoryCase{L"MAP\\USER\\Antares.map",
                      LaunchSource::SinglePlayerCustomMap}}) {
    const auto direct = RefineLaunchOrigin(
        {LaunchSource::SinglePlayerMap, SessionEligibility::Eligible},
        value.relative);
    const auto loaded = RefineLaunchOrigin(
        {LaunchSource::LoadMapUnresolved, SessionEligibility::Unknown},
        value.relative);
    Require(direct.source == value.expected &&
                loaded.source == value.expected &&
                direct.eligibility == SessionEligibility::Eligible &&
                loaded.eligibility == SessionEligibility::Eligible,
            "direct and loaded fixed maps use one canonical category");
}
```

Add explicit assertions that `Map\Userland\X.map` falls back to
`SinglePlayerMap`, `LoadCampaign` becomes `Campaign`, random remains
`RandomMap`, and an excluded online origin remains unchanged even for
`Map\User\Antares.map`.

- [ ] **Step 2: Push the RED test commit and verify the expected CI failure**

Run:

```bash
git add tests/MapSessionPolicyTests.cpp
git commit -m "test: require canonical map categories"
git push origin main
gh run watch --exit-status
```

Expected: the Win32 Build succeeds and CTest fails at
`direct and loaded fixed maps use one canonical category`; policy-fixture
rejection must not be the failure.

- [ ] **Step 3: Declare and implement the minimal pure classifier**

Add to `MapSessionPolicy.h`:

```cpp
LaunchSource CanonicalCompletionSource(
    LaunchSource admittedSource,
    std::wstring_view relativeIdentifier) noexcept;
```

In `MapSessionPolicy.cpp`, implement an ASCII-only path-prefix comparator that
folds `A-Z` to lowercase, treats `/` as `\`, and compares prefixes that include
the trailing separator:

```cpp
wchar_t FoldPathUnit(wchar_t value) noexcept {
    if (value == L'/') return L'\\';
    if (value >= L'A' && value <= L'Z') return value + (L'a' - L'A');
    return value;
}

bool HasPathPrefix(std::wstring_view value,
                   std::wstring_view prefix) noexcept {
    if (value.size() <= prefix.size()) return false;
    for (std::size_t index = 0u; index < prefix.size(); ++index) {
        if (FoldPathUnit(value[index]) != FoldPathUnit(prefix[index])) {
            return false;
        }
    }
    return true;
}
```

Implement category selection in this order:

```cpp
if (IsRandomMapIdentifier(relativeIdentifier)) return LaunchSource::RandomMap;
if (admittedSource == LaunchSource::Campaign ||
    admittedSource == LaunchSource::LoadCampaign) {
    return LaunchSource::Campaign;
}
if (HasPathPrefix(relativeIdentifier, L"Map\\Multiplayer\\"))
    return LaunchSource::SinglePlayerMultiplayerMap;
if (HasPathPrefix(relativeIdentifier, L"Map\\User\\"))
    return LaunchSource::SinglePlayerCustomMap;
return LaunchSource::SinglePlayerMap;
```

Update `RefineLaunchOrigin` so online exclusion returns first; random identity
still resolves a missed selector; `LoadMapUnresolved` becomes eligible only
after identity confirmation; every eligible non-online origin then receives
`CanonicalCompletionSource(...)`.

- [ ] **Step 4: Run the focused and full test suites on Win32 CI**

Run:

```bash
git add src/victory/MapSessionPolicy.h src/victory/MapSessionPolicy.cpp \
  tests/MapSessionPolicyTests.cpp
git commit -m "fix: canonicalize map categories from identity"
git push origin main
gh run watch --exit-status
```

Expected: archive integration, Build, mutation-policy checks, CTest, Package,
PE32/package verification, and upload all pass.

---

### Task 2: Safe schema-version-1 normalization

**Files:**
- Modify: `src/completion/CompletionRecord.h`
- Modify: `src/completion/CompletionRecord.cpp`
- Modify: `src/completion/CompletionJson.cpp`
- Modify: `src/completion/CompletionStore.h`
- Modify: `src/completion/CompletionStore.cpp`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Test: `tests/CompletionRecordTests.cpp`
- Test: `tests/CompletionStoreTests.cpp`

**Interfaces:**
- Consumes: `CanonicalCompletionSource` from Task 1 and a strictly decoded schema-version-1 snapshot.
- Produces: public `std::optional<std::wstring> StrictUtf8ToWide(std::string_view value) noexcept`, `CompletionStoreMode::ReadOnlyNormalizationFailed`, and one verified commit helper shared by migration and `AddIfAbsent`.

- [ ] **Step 1: Add failing normalization and failure-policy tests**

First make the normal `Record("a")` fixture canonical for its
`Map\User\...` identity:

```cpp
record.launchSource = LaunchSource::SinglePlayerCustomMap;
```

Add a `LegacyRecord` helper that overrides the source. Add tests proving:

```cpp
FakeFileOps files;
const auto legacy = LegacyRecord("antares", LaunchSource::SinglePlayerMap);
const auto oldBytes = Database(legacy);
files.files[L"main"] = oldBytes;
auto store = MakeStore(files);
const auto load = store.Load();
Require(load.mode == CompletionStoreMode::WritableLoaded &&
            files.writeCalls == 1u && files.replaceCalls == 1u &&
            files.files[L"backup"] == oldBytes &&
            store.Snapshot().records.front().launchSource ==
                LaunchSource::SinglePlayerCustomMap &&
            store.Snapshot().records.front().completedAtUtc ==
                legacy.completedAtUtc,
        "writable legacy source is atomically normalized");
```

Add separate cases for `LoadSinglePlayerMap` under `Map\Multiplayer`,
`LoadCampaign` becoming `Campaign`, an already canonical main causing zero
writes, a legacy valid backup remaining `ReadOnlyBackup` with zero writes, and
an injected normalization write failure returning
`ReadOnlyNormalizationFailed`, retaining the original decoded snapshot, and
rejecting later `AddIfAbsent` calls.

In `CompletionRecordTests.cpp`, add invalid UTF-8, embedded NUL, and valid
non-ASCII round-trip cases for the soon-to-be-public `StrictUtf8ToWide`.

- [ ] **Step 2: Push the RED test commit and verify the expected CI failure**

Run:

```bash
git add tests/CompletionRecordTests.cpp tests/CompletionStoreTests.cpp
git commit -m "test: require safe stored-category normalization"
git push origin main
gh run watch --exit-status
```

Expected: Build fails only because `StrictUtf8ToWide` and
`ReadOnlyNormalizationFailed` are not yet public/defined. This is the RED gate;
mutation-policy rejection is not an acceptable substitute.

- [ ] **Step 3: Share the strict UTF converter without changing validation**

Declare in `CompletionRecord.h`:

```cpp
std::optional<std::wstring> StrictUtf8ToWide(
    std::string_view value) noexcept;
```

Move the existing implementation unchanged from the anonymous namespace in
`CompletionJson.cpp` to `CompletionRecord.cpp`. Keep `CompletionJson.cpp`
calling the public function so JSON string and record validation behavior is
byte-for-byte equivalent.

- [ ] **Step 4: Add one shared verified snapshot transaction**

Add to `CompletionStore` private methods:

```cpp
CompletionAddResult CommitSnapshot(
    CompletionDatabaseSnapshot candidate,
    std::set<std::string> candidateIds,
    bool replaceExisting);
```

Move the existing encode, temporary write/flush, bounded read-back, decode,
canonical re-encode comparison, replace/move, and no-allocation publication
sequence from `AddIfAbsent` into this helper. `replaceExisting == true` calls
`ReplaceWithBackup`; `false` calls `MoveFirstWriteThrough`. `AddIfAbsent`
continues to perform its mode and duplicate gates before calling the helper.

- [ ] **Step 5: Normalize only writable valid main data**

Add `ReadOnlyNormalizationFailed` to `CompletionStoreMode` and map it to
`read-only-normalization-failed` in `DiagnosticRuntime.cpp`. Treat this mode
like `ReadOnlyBackup` for log severity and continued read-only startup, and emit
the explicit warning
`completion-store normalization failed; original data loaded read-only; future writes are disabled; manual handling required`.
Do not add it to the modes that call `AbortStart()`.

When `Load()` decodes a valid main:

```cpp
auto original = std::move(mainJson.snapshot);
auto normalized = original;
bool changed = false;
for (auto& record : normalized.records) {
    const auto relative = StrictUtf8ToWide(record.relativeId);
    if (!relative.has_value()) {
        Publish(std::move(original));
        mode_ = CompletionStoreMode::ReadOnlyNormalizationFailed;
        return LoadResult(mode_, CompletionJsonFailure::Utf8,
                          ERROR_INVALID_DATA, snapshot_.records.size());
    }
    const auto canonical = CanonicalCompletionSource(
        record.launchSource, *relative);
    if (canonical != record.launchSource) {
        record.launchSource = canonical;
        changed = true;
    }
}
```

If unchanged, publish normally without writing. If changed, build the stable-ID
set before file commit and call `CommitSnapshot(..., true)`. On success return
`WritableLoaded`. On failure publish the untouched `original`, set
`ReadOnlyNormalizationFailed`, return the transaction error, and block later
writes. Never normalize a backup-loaded snapshot.

- [ ] **Step 6: Run the focused and full test suites on Win32 CI**

Run:

```bash
git add src/completion/CompletionRecord.h src/completion/CompletionRecord.cpp \
  src/completion/CompletionJson.cpp src/completion/CompletionStore.h \
  src/completion/CompletionStore.cpp src/runtime/DiagnosticRuntime.cpp \
  tests/CompletionRecordTests.cpp tests/CompletionStoreTests.cpp
git commit -m "fix: normalize stored map categories safely"
git push origin main
gh run watch --exit-status
```

Expected: every workflow step passes. Inspect the CTest output if any test
fails; do not proceed on a packaging-only success.

---

### Task 3: Runtime equivalence regression and release candidate evidence

**Files:**
- Modify: `tests/CompletionCandidateCoordinatorTests.cpp`
- Modify: `tests/CompletionRuntimeFlowTests.cpp`
- Modify: `docs/research/phase-4-completion-persistence-build-report.md`

**Interfaces:**
- Consumes: canonical `RefineLaunchOrigin` output from Task 1 and the normalized store from Task 2.
- Produces: regression proof that direct and loaded wins enqueue identical category metadata, plus a hash-audited Win32 release candidate.

- [ ] **Step 1: Add the runtime equivalence regression**

In both runtime-flow test files, derive the loaded origin from the same identity
used by the candidate:

```cpp
const auto loaded = RefineLaunchOrigin(
    {LaunchSource::LoadMapUnresolved, SessionEligibility::Unknown},
    L"Map\\User\\Battle of the Gods.map");
const auto direct = RefineLaunchOrigin(
    {LaunchSource::SinglePlayerMap, SessionEligibility::Eligible},
    L"Map\\User\\Battle of the Gods.map");
Require(loaded.source == LaunchSource::SinglePlayerCustomMap &&
            direct.source == loaded.source,
        "direct and loaded candidates share canonical custom source");
```

Update the old expectation that retained `LoadSinglePlayerMap`; both candidate
records must now contain `SinglePlayerCustomMap`. Keep the stable ID and
first-completion-time assertions unchanged.

- [ ] **Step 2: Run the entire release workflow**

Run:

```bash
git add tests/CompletionCandidateCoordinatorTests.cpp \
  tests/CompletionRuntimeFlowTests.cpp
git commit -m "test: prove direct and loaded category equivalence"
git push origin main
gh run watch --exit-status
```

Expected: all GitHub Actions steps pass, including Win32 `/W4 /WX`, CTest,
mutation fixtures, package integration, PE32 verification, and artifact upload.

- [ ] **Step 3: Download and verify the candidate without deploying it**

Run the repository's existing artifact-download procedure for the successful
workflow, then verify:

```bash
sha256sum artifacts/CampaignCompletionDebug.asi \
  artifacts/CampaignCompletionDebug.ini \
  artifacts/Plugin_SU.zip
git status --short --branch
```

Expected: three stable SHA-256 values are recorded; `main` matches
`origin/main`; only the pre-existing untracked `artifacts/` and
`research/evidence/save-samples/` remain. Do not replace the live archive while
the game is running.

- [ ] **Step 4: Record build evidence and commit it**

Append the successful workflow/run IDs, commit ID, ASI/INI/candidate-ZIP hashes,
and the exact passing gates to
`docs/research/phase-4-completion-persistence-build-report.md`.

Run:

```bash
git add docs/research/phase-4-completion-persistence-build-report.md
git commit -m "docs: verify canonical map category build"
git push origin main
```

Expected: documentation-only workflow behavior is understood, `main` is clean
apart from intentional untracked evidence, and no game file has changed.

- [ ] **Step 5: Stop at the deployment gate**

Inspect `S4_Main.exe` read-only. If it is running, report the candidate hashes
and ask the user to close the game. Only after the user confirms closure may
the existing immutable-backup, hash-gated archive replacement procedure be
run. After deployment, startup should normalize the current Antares record to
`single-player-custom-map` while preserving its original completion timestamp.
