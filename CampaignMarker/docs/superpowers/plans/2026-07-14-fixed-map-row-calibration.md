# Fixed-Map Row Calibration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Win32 Phase 5A calibration artifact that safely correlates the completed Aeneas and Antares records with public S4ModApi GUI-element observations, proves callback order and scrolling behavior, and keeps completion-marker drawing disabled.

**Architecture:** Load a filtered immutable marker-candidate index from the existing completion-store snapshot, then feed category-bounded public GUI observations into a calibration coordinator. Write only matching candidate features and frame boundaries to a new 256 KiB per-process trace below the project-owned plugin data directory; do not draw, create controls, or add an internal hook. The resulting live evidence is the required input for a separate Phase 5B rendering plan.

**Tech Stack:** C++17, Win32 strict UTF conversion and bounded SEH callback-text copy, S4ModApi 2.3.2 public listeners, CMake/CTest, MSVC Win32 `/W4 /WX`, PowerShell packaging/policy checks, GitHub Actions.

## Global Constraints

- Work directly on `main`, as explicitly requested; do not create a worktree or feature branch.
- The game is currently running. Do not deploy, replace the SU archive, or alter any game-directory file until the user closes the game and explicitly approves deployment.
- Read/copy access to game and SU files remains allowed; never modify maps, saves, game resources, or unrelated plugin files.
- Phase 5A must keep `CompletionDetection=1`, `CompletionStorage=1`, and `CompletionMarkers=0`.
- Use only public S4ModApi GUI-element and UI-frame callbacks; add no internal menu-model hook, process patch, Lua write, game-data write, or mouse-intercepting custom control.
- Do not use row text as a completion database key. Stable identity remains the normalized relative map path already stored in `completed_maps.json`.
- Log only candidate map display names and bounded public GUI fields; never log raw pointers, memory contents, module bases, absolute game paths, or unrelated UI text.
- The marker-calibration trace must use `CREATE_NEW`, remain below 256 KiB, and never overwrite an earlier process trace.
- Callback work must be bounded and exception-contained. No filesystem operation occurs in the successful per-element path except the dedicated calibration trace write for a matching candidate.
- A missing/ambiguous candidate, invalid encoding, unknown tab, wrong page set, or invalid rectangle fails closed and produces no marker claim.
- Existing victory detection, completion persistence, backup recovery, shutdown, and controlled-stop behavior remain regression gates.
- The generated artifact remains x86 PE32 and retains the existing two-file package layout.

---

## File structure

- `src/diagnostics/MarkerCalibrationTrace.h`: bounded trace policy and writer interface.
- `src/diagnostics/MarkerCalibrationTrace.cpp`: unique path creation, allowlist validation, 256 KiB cap, synchronized writes, and close.
- `tests/MarkerCalibrationTraceTests.cpp`: path uniqueness, allowlist, size cap, and non-overwrite tests.
- `src/marker/CompletionMarkerIndex.h`: immutable candidate types and lookup interface.
- `src/marker/CompletionMarkerIndex.cpp`: persisted-record filtering, strict decoding, category selection, filename/display-name guard, and atomic snapshot publication.
- `tests/CompletionMarkerIndexTests.cpp`: fixed/random/online/category/duplicate and malformed-record coverage.
- `src/marker/FixedMapRowCalibration.h`: bounded GUI observation value type, safe text-copy interface, and calibration coordinator.
- `src/marker/FixedMapRowCalibration.cpp`: exact-page/tab state, callback sequencing, lossless ANSI decoding, candidate lookup, and trace records.
- `tests/FixedMapRowCalibrationTests.cpp`: text bounds, state reset, category isolation, ambiguity, geometry, and frame-order coverage.
- `src/runtime/S4Listeners.h/.cpp`: forward public frame/GUI/page/tab observations to calibration without changing mouse behavior.
- `src/runtime/DiagnosticRuntime.h/.cpp`: own and initialize trace/index/calibration components in dependency-safe order.
- `config/CampaignCompletionDebug.ini`: version `0.5.0`, Phase 5A mode, calibration enabled, markers disabled.
- `CMakeLists.txt`: link the new production sources and test translation units.
- `tests/TestMain.cpp`: invoke the three new test groups.
- `tests/RuntimePolicyTests.cpp`: enforce public-only calibration, disabled drawing, trace location, runtime ownership, and shutdown ordering.
- `docs/research/phase-5a-fixed-map-row-calibration-report.md`: static, CI, deployment, and live evidence with explicit GO/NO-GO status.

---

### Task 1: Bounded marker-calibration trace

**Files:**
- Create: `src/diagnostics/MarkerCalibrationTrace.h`
- Create: `src/diagnostics/MarkerCalibrationTrace.cpp`
- Create: `tests/MarkerCalibrationTraceTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: an existing project data directory, process ID, and one single-line allowlisted UTF-8 record.
- Produces: `bool IsMarkerCalibrationRootAdmitted(DWORD, bool) noexcept`, `bool IsMarkerCalibrationRecordAllowed(std::string_view) noexcept`; `MarkerCalibrationTrace::Open(const std::filesystem::path&, DWORD)`; `Write(std::string_view)`; `Close() noexcept`; and `path() const noexcept`.

- [ ] **Step 1: Add the failing trace policy and file-lifecycle tests**

Create `tests/MarkerCalibrationTraceTests.cpp` with a `RunMarkerCalibrationTraceTests()` entry point. Cover these exact records:

```cpp
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
Require(!IsMarkerCalibrationRecordAllowed("marker-candidate=0x1234"),
        "pointer-shaped token is rejected");
Require(!IsMarkerCalibrationRecordAllowed(
            "marker-candidate=name-Aeneas\nmodule name=S4_Main.exe"),
        "newline and unrelated module data are rejected");
Require(!IsMarkerCalibrationRecordAllowed(
            "marker-candidate=path-F:\\Games\\Aeneas.map"),
        "absolute path is rejected");
```

Use a temporary existing directory to prove the first writer creates
`marker-calibration-pid-4242.trace`, the second creates
`marker-calibration-pid-4242-1.trace`, the first remains byte-identical, and a
missing/file root is rejected. Test `IsMarkerCalibrationRootAdmitted` directly
with directory, file, unresolved-final-path, and reparse attributes. Write valid records until the next line
would exceed `262144` bytes; require that write to return false and the file size
to remain at or below `262144`.

Declare and call `RunMarkerCalibrationTraceTests()` from `tests/TestMain.cpp`,
and add the new test/source files to the test target in `CMakeLists.txt`.

- [ ] **Step 2: Commit the RED test and verify the expected Win32 CI failure**

Run:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/MarkerCalibrationTraceTests.cpp
git commit -m "test: require bounded marker calibration trace"
git push origin main
gh run watch --exit-status
```

Expected: compilation fails because `diagnostics/MarkerCalibrationTrace.h` and
its interfaces do not exist. The archive-integration and mutation fixtures must
not be the failure source.

- [ ] **Step 3: Implement the minimal bounded writer**

Define the header as:

```cpp
#pragma once

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace campaign_completion {

inline constexpr std::size_t kMarkerCalibrationMaximumBytes = 256u * 1024u;
inline constexpr std::size_t kMarkerCalibrationMaximumRecordBytes = 2048u;

bool IsMarkerCalibrationRecordAllowed(std::string_view record) noexcept;
bool IsMarkerCalibrationRootAdmitted(DWORD attributes,
                                     bool finalPathAvailable) noexcept;

class MarkerCalibrationTrace final {
public:
    bool Open(const std::filesystem::path& root, DWORD processId);
    bool Write(std::string_view record);
    void Close() noexcept;
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
    std::size_t bytesWritten_ = 0u;
};

}  // namespace campaign_completion
```

Implement allowlisted prefixes `marker-state=`, `marker-candidate=`, and
`marker-frame=`. Reject empty lines, CR/LF, records over 2048 bytes, and
case-insensitive tokens `0x`, `module name=`, `base=`, `memory`, `pointer`,
`path-`, and `:\`. Reuse the root admission checks from `CaptureTrace` without
weakening that class. Create up to 100 unique PID/suffix names with `CREATE_NEW`,
write CRLF-terminated lines only when the resulting total is at most 262144,
and flush each accepted line.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

Run on Windows or through the pushed GitHub workflow:

```bash
git add CMakeLists.txt src/diagnostics/MarkerCalibrationTrace.h \
  src/diagnostics/MarkerCalibrationTrace.cpp \
  tests/MarkerCalibrationTraceTests.cpp tests/TestMain.cpp
git commit -m "feat: add bounded marker calibration trace"
git push origin main
gh run watch --exit-status
```

Expected: Build and `campaign_completion_tests` pass; package contents remain
the existing ASI and INI only.

---

### Task 2: Immutable completion-marker candidate index

**Files:**
- Create: `src/marker/CompletionMarkerIndex.h`
- Create: `src/marker/CompletionMarkerIndex.cpp`
- Create: `tests/CompletionMarkerIndexTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `CompletionDatabaseSnapshot`, `CompletionRecord`, and current `FixedMapListKind` plus decoded row label.
- Produces: `MarkerCandidate`, `MarkerCandidateSnapshot`, `FixedMapListKind FixedListKindForRecord(const CompletionRecord&) noexcept`, `CompletionMarkerIndex::Publish(const CompletionDatabaseSnapshot&) noexcept`, and `Find(FixedMapListKind, std::wstring_view) const`.

- [ ] **Step 1: Add failing record-filter and lookup tests**

Define test records for:

```cpp
Record("map:map\\singleplayer\\aeneas.map",
       "Map\\Singleplayer\\Aeneas.map", "Aeneas",
       CompletionMapKind::Fixed, LaunchSource::SinglePlayerMap);
Record("map:map\\multiplayer\\alien.map",
       "Map\\Multiplayer\\Alien.map", "Alien",
       CompletionMapKind::Fixed,
       LaunchSource::SinglePlayerMultiplayerMap);
Record("map:map\\user\\antares.map",
       "Map\\User\\Antares.map", "Antares",
       CompletionMapKind::Fixed, LaunchSource::SinglePlayerCustomMap);
```

Publish all three and require one result only in the matching list, using an
ordinal case-insensitive row label. Add rejection tests for random kind,
`OnlineMultiplayer`, `LoadOnlineMultiplayer`, campaign paths, invalid UTF-8,
empty/oversized display names, a display name unequal to the relative filename
stem, a `Campaign` source even when its path begins `Map\Singleplayer`, and
`Map\Userland` prefix confusion. Publish two valid Antares records and
require `Find(Custom, L"Antares")` to return two candidates so the caller can
fail closed on ambiguity. Publish a replacement snapshot and require the old
candidate to disappear.

- [ ] **Step 2: Commit the RED test and verify the expected failure**

Run:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionMarkerIndexTests.cpp
git commit -m "test: require fixed-map marker candidate index"
git push origin main
gh run watch --exit-status
```

Expected: compilation fails on the missing marker-index header/interface.

- [ ] **Step 3: Implement strict filtering and immutable publication**

Define:

```cpp
struct MarkerCandidate final {
    std::string stableId;
    std::wstring displayName;
    FixedMapListKind listKind = FixedMapListKind::Unknown;
};

using MarkerCandidateSnapshot = std::vector<MarkerCandidate>;

class CompletionMarkerIndex final {
public:
    void Publish(const CompletionDatabaseSnapshot& snapshot) noexcept;
    MarkerCandidateSnapshot Find(FixedMapListKind listKind,
                                 std::wstring_view rowLabel) const noexcept;

private:
    mutable std::mutex mutex_;
    MarkerCandidateSnapshot candidates_;
};
```

Use `StrictUtf8ToWide` for `relativeId` and `displayName`. Recognize only complete
case-insensitive, slash-insensitive prefixes `Map\Singleplayer\`,
`Map\Multiplayer\`, and `Map\User\`. Reject online sources before category
selection. Require the source/category pairs `SinglePlayerMap` plus
`Map\Singleplayer`, `SinglePlayerMultiplayerMap` plus `Map\Multiplayer`, or
`SinglePlayerCustomMap` plus `Map\User`. Require fixed kind, `.map` extension, a
filename stem ordinal-equal to the display name, and no control character or
more than 256 UTF-16 code units in the name. Build a candidate vector off-lock
and swap it under one mutex.
`Find` copies only ordinal case-insensitive matches from the requested known
list; it never returns a candidate for `Unknown`.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

Run:

```bash
git add CMakeLists.txt src/marker/CompletionMarkerIndex.h \
  src/marker/CompletionMarkerIndex.cpp tests/CompletionMarkerIndexTests.cpp \
  tests/TestMain.cpp
git commit -m "feat: index fixed-map marker candidates"
git push origin main
gh run watch --exit-status
```

Expected: all Win32 tests and workflow gates pass, including random/online
exclusion regressions.

---

### Task 3: Public fixed-map row calibration coordinator

**Files:**
- Create: `src/marker/FixedMapRowCalibration.h`
- Create: `src/marker/FixedMapRowCalibration.cpp`
- Create: `tests/FixedMapRowCalibrationTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `CompletionMarkerIndex`, `MarkerCalibrationTrace`, exact `PageSnapshot`, current `FixedMapListKind`, public UI-frame metadata, and a copied `MarkerGuiElement`.
- Produces: safe `CopyBoundedGuiText`, `DecodeMenuTextLossless`, and `FixedMapRowCalibration::{ObservePages, ObserveListKind, ObserveFrame, ObserveElement, Disable}`.

- [ ] **Step 1: Add failing callback-safety and coordinator tests**

Define the transport value without raw pointers:

```cpp
struct MarkerGuiElement final {
    DWORD surfaceWidth = 0u;
    DWORD surfaceHeight = 0u;
    WORD containerType = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
    WORD valueLink = 0u;
    WORD textStyle = 0u;
    std::string text;
};
```

Test `CopyBoundedGuiText` with null, `"Aeneas"`, an embedded control character,
and 257 non-NUL bytes. The two NUL-terminated byte strings copy successfully;
null and unterminated input fail. Test lossless ACP-to-UTF-16 conversion with
ASCII, require the embedded control character to fail validation, and require
rejected input not to reach the trace sink.

Inject a `std::function<bool(std::string_view)>` trace sink into the coordinator.
Publish Aeneas and Antares candidates, then prove:

- no pages or a non-exact page set produces no record;
- exact `{4,22,23,25}` plus `Single` records only Aeneas;
- switching to `Custom` records only Antares;
- `Unknown` list and leaving the exact page set reset candidate state;
- invalid/zero/out-of-surface rectangles are rejected;
- duplicate candidate lookup records `match-ambiguous` and never `match-unique`;
- a unique candidate record includes sequence, generation, category, name,
  rectangle, surface, value-link, container, and text-style;
- the next `ObserveFrame` writes one frame boundary with the first/last candidate
  sequence, then clears the pending range;
- `Disable()` makes every subsequent method inert.

- [ ] **Step 2: Commit the RED test and verify the expected failure**

Run:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/FixedMapRowCalibrationTests.cpp
git commit -m "test: require public fixed-map row calibration"
git push origin main
gh run watch --exit-status
```

Expected: compilation fails because the calibration coordinator does not exist.

- [ ] **Step 3: Implement safe copy, decoding, state, and bounded records**

Declare:

```cpp
class FixedMapRowCalibration final {
public:
    using TraceSink = std::function<bool(std::string_view)>;

    FixedMapRowCalibration(const CompletionMarkerIndex& index,
                           TraceSink trace);
    void ObservePages(const PageSnapshot& snapshot) noexcept;
    void ObserveListKind(FixedMapListKind kind) noexcept;
    void ObserveFrame(DWORD page, INT32 pillarboxWidth) noexcept;
    void ObserveElement(const MarkerGuiElement& element) noexcept;
    void Disable() noexcept;
};
```

Implement `CopyBoundedGuiText(const char*, std::string&) noexcept` in a dedicated
MSVC SEH function that copies at most 256 bytes and succeeds only on a NUL before
the limit. The function must not allocate or own C++ objects inside the
`__try/__except` region; copy into a fixed `std::array<char,257>` first, then
assign after the protected region succeeds.

Implement `DecodeMenuTextLossless` with `MultiByteToWideChar(CP_ACP,
MB_PRECOMPOSED, ...)`, then round-trip using `WideCharToMultiByte(CP_ACP,
WC_NO_BEST_FIT_CHARS, ..., &usedDefault)` and require byte equality plus
`usedDefault == FALSE`. Reject controls and more than 256 decoded units.

`ObservePages` accepts only an active-page vector exactly equal to
`{4,22,23,25}`; every change increments generation and resets the pending
candidate sequence range. `ObserveListKind` accepts only a known list while the
page gate is active and likewise advances generation on change. Every frame and
element callback increments a monotonic sequence. Candidate observations query
the index, validate `width/height > 0`, `x + width <= surfaceWidth`, and
`y + height <= surfaceHeight`, then emit a hyphen-delimited, CR/LF-free record.
`ObserveFrame` emits only when at least one candidate occurred since the prior
frame, which keeps a two-minute run below the trace cap.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

Run:

```bash
git add CMakeLists.txt src/marker/FixedMapRowCalibration.h \
  src/marker/FixedMapRowCalibration.cpp \
  tests/FixedMapRowCalibrationTests.cpp tests/TestMain.cpp
git commit -m "feat: calibrate public fixed-map rows"
git push origin main
gh run watch --exit-status
```

Expected: all tests pass with no marker rendering or custom UI source linked.

---

### Task 4: Runtime integration with markers still disabled

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `src/completion/CompletionRecord.h`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `CMakeLists.txt`
- Modify: `tests/CompletionRecordTests.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`

**Interfaces:**
- Consumes: store snapshot, project data directory, public UI-frame callback values, GUI-element callback values, page snapshots, and approved tab attribution.
- Produces: an initialized Phase 5A trace/index/coordinator pipeline that shuts down before listener removal and never draws.

- [ ] **Step 1: Add failing runtime ownership, policy, and shutdown tests**

Extend `RuntimePolicyTests.cpp` to require:

```cpp
"Version=0.5.0"
"DiagnosticMode=FixedMapRowCalibration"
"MarkerCalibration=1"
"CompletionDetection=1"
"CompletionStorage=1"
"CompletionMarkers=0"
```

Require `DiagnosticRuntime.h` to own `MarkerCalibrationTrace`,
`CompletionMarkerIndex`, and `FixedMapRowCalibration` in that dependency order.
Require the runtime to call `markerIndex_->Publish(store_->Snapshot())` only
after a successful store load, open the marker trace below `paths_->dataDirectory`,
and disable calibration before `listeners_.Stop()` while closing its trace only
after listeners stop.

Require `kCompletionPluginVersion == "0.5.0"` so a victory recorded by the
calibration build identifies the actual plugin version without rewriting older
records.

Require `S4Listeners` to pass `lpSurface`/pillarbox through the templated public
UI callback, pass a value-copied GUI element through `CopyBoundedGuiText`, feed
each emitted `PageSnapshot` and approved tab change to the coordinator, and
contain all calibration exceptions. Forbid `CreateCustomUiElement`,
an exact `AddGuiBltListener(` call, `IDirectDrawSurface7::GetDC`, `DrawIcon`,
`Polyline`, `CompletionMarkerRenderer`, and any new internal hook token in the
Phase 5A runtime sources. The already required `AddGuiElementBltListener` remains
the public observation source.

Require the calibration trace root to be exactly the already resolved
`paths_->dataDirectory`; do not add another path setting or derive a game-root
location.

- [ ] **Step 2: Commit the RED runtime-policy test and verify failure**

Run:

```bash
git add tests/RuntimePolicyTests.cpp
git commit -m "test: require marker calibration runtime policy"
git push origin main
gh run watch --exit-status
```

Expected: CTest fails on missing version/mode/runtime ownership assertions while
the ASI still compiles.

- [ ] **Step 3: Wire the calibration components into runtime startup**

In `DiagnosticRuntime`, create and publish in this order after `store_->Load()`:

```cpp
markerTrace_ = std::make_unique<MarkerCalibrationTrace>();
const bool markerTraceOpen =
    markerTrace_->Open(paths_->dataDirectory, GetCurrentProcessId());
if (!markerTraceOpen) {
    logger_.Write(LogLevel::Warning,
                  "marker calibration trace unavailable; calibration disabled");
}
markerIndex_ = std::make_unique<CompletionMarkerIndex>();
markerIndex_->Publish(store_->Snapshot());
markerCalibration_ = std::make_unique<FixedMapRowCalibration>(
    *markerIndex_, [this](std::string_view record) {
        return markerTrace_ != nullptr && markerTrace_->Write(record);
    });
if (!markerTraceOpen) {
    markerCalibration_->Disable();
}
```

Pass `*markerCalibration_` to `S4Listeners::Start`. Change the UI callback path
to preserve its public arguments:

```cpp
template <DWORD Page>
static HRESULT S4HCALL OnUiFrameFor(LPDIRECTDRAWSURFACE7 surface,
                                    INT32 pillarboxWidth, LPVOID) {
    DispatchUiFrame(Page, surface, pillarboxWidth);
    return S_OK;
}
```

In `ObserveGuiElement`, copy only the documented scalar fields plus bounded text
into `MarkerGuiElement`; do not retain `element` or its text pointer. On page
snapshot publication call `ObservePages`; after `listAttribution_` changes on an
approved tab click call `ObserveListKind`; on every UI frame call
`ObserveFrame(page, pillarboxWidth)`.

During controlled stop call `markerCalibration_->Disable()` before
`listeners_.Stop()`. Reset calibration, index, and trace after worker drain and
listener quiescence; close the trace before resetting it. Mirror the same safe
ordering in `AbortStart()`.

Update `kCompletionPluginVersion`, the runtime banner, and the INI version to
`0.5.0`; set the runtime mode to `fixed-map-row-calibration` and the INI fields
to the exact values asserted in Step 1. Add all new production sources to the
ASI target.

- [ ] **Step 4: Run all workflow gates and commit GREEN**

Run:

```bash
git add CMakeLists.txt config/CampaignCompletionDebug.ini \
  src/completion/CompletionRecord.h \
  src/runtime/DiagnosticRuntime.h src/runtime/DiagnosticRuntime.cpp \
  src/runtime/S4Listeners.h src/runtime/S4Listeners.cpp \
  tests/CompletionRecordTests.cpp tests/RuntimePolicyTests.cpp
git commit -m "feat: integrate fixed-map row calibration"
git push origin main
gh run watch --exit-status
```

Expected: archive integration, Build, process-mutation fixtures, CTest, Package,
PE32/package verification, and artifact upload all pass. The packaged INI still
contains `CompletionMarkers=0`.

---

### Task 5: Candidate artifact audit and Phase 5A build report

**Files:**
- Create: `docs/research/phase-5a-fixed-map-row-calibration-report.md`
- Use without tracking: `artifacts/phase-5a-row-calibration/`

**Interfaces:**
- Consumes: the successful GitHub Actions run, downloaded artifact, source tree, and package policy.
- Produces: a reproducible candidate audit and build report; no deployment.

- [ ] **Step 1: Download the exact successful artifact and record identities**

Run:

```bash
run_id="$(gh run list --workflow build-debug-asi.yml --branch main \
  --status success --limit 1 --json databaseId --jq '.[0].databaseId')"
mkdir -p "artifacts/phase-5a-row-calibration/run-$run_id"
gh run download "$run_id" --name CampaignCompletionDebug-Win32 \
  --dir "artifacts/phase-5a-row-calibration/run-$run_id"
sha256sum "artifacts/phase-5a-row-calibration/run-$run_id/"*.zip
unzip -l "artifacts/phase-5a-row-calibration/run-$run_id/"*.zip
```

Extract into a new directory below the same untracked run directory. Record the
ZIP, ASI, and INI SHA-256 values, ASI PE machine `0x014c`, workflow run/job/artifact
IDs, package entry list, and configuration fields in the report.

- [ ] **Step 2: Run repository policy checks against the candidate source**

Run from Windows PowerShell:

```powershell
cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
./tools/verify_no_process_patch.ps1 `
  -Binary build/Debug/CampaignCompletionDebug.asi `
  -SourceRoot .
./tools/test_settlers_united_artifact.ps1
```

Expected: every command exits zero. The report must say Phase 5A observes
candidate rows only, writes one project-owned bounded trace, does not render a
marker, and does not change completion semantics.

- [ ] **Step 3: Commit the verified build report**

Use `apply_patch` to create the report with the exact values collected above,
then run:

```bash
git add docs/research/phase-5a-fixed-map-row-calibration-report.md
git commit -m "docs: verify fixed-map row calibration build"
git push origin main
```

Expected: `main` and `origin/main` match; only the intentional untracked
`artifacts/` and `research/evidence/save-samples/` remain.

---

### Task 6: Guarded deployment and live calibration gate

**Files:**
- Modify after evidence: `docs/research/phase-5a-fixed-map-row-calibration-report.md`
- Store without tracking: `artifacts/phase-5a-row-calibration/live/`

**Interfaces:**
- Consumes: audited candidate artifact, explicit user approval, closed game/SU processes, guarded SU archive installer, and user-driven menu actions.
- Produces: accepted row signatures/callback order with a GO decision, or a bounded NO-GO report with markers still disabled.

- [ ] **Step 1: Stop and request deployment authorization**

Report the audited ZIP/ASI/INI hashes and ask the user to close the game and
Settlers United. Do not run an installer until the user explicitly says they
are closed and approves deployment.

- [ ] **Step 2: Deploy only through the existing guarded archive procedure**

After approval, use the existing project installer with the audited ASI and INI.
Require it to verify the immutable original backup hash
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`,
preserve every non-project ZIP entry, replace only the project ASI/configuration,
and report the patched archive and embedded ASI hashes. Independently reopen the
archive read-only and reproduce those hashes before asking the user to launch.

- [ ] **Step 3: Collect the approved user-driven calibration sequence**

Ask the user to perform one action at a time:

1. open Single Player Maps and explicitly click its tab;
2. locate Aeneas, then show normal, hover, and selected states;
3. scroll Aeneas out of view and back into view;
4. open Custom Maps, explicitly click its tab, and repeat for Antares;
5. open Multiplayer Maps and scroll once as the no-completed-record control;
6. return normally and close the game.

After each reported action, copy only the current process's
`marker-calibration-pid-<pid>.trace` into the untracked live evidence directory
and inspect it read-only. Never terminate the game.

- [ ] **Step 4: Apply the GO/NO-GO criteria and document exact evidence**

GO requires all of the following:

- Aeneas appears only under `list-single` and Antares only under `list-custom`;
- each state/scroll cycle exposes a repeatable candidate feature signature;
- candidate rectangles move/disappear/reappear with the row and stay within the
  reported surface;
- sequence/frame records establish the callback order needed for frame-end draw;
- Multiplayer Maps produces no Aeneas/Antares candidate;
- the trace remains at or below 262144 bytes;
- no calibration, listener, completion-store, input, or shutdown error occurs;
- the game remains responsive and exits normally.

Any false candidate, ambiguous unresolvable signature, stale rectangle, unsafe
callback order, trace overflow, input regression, or exit error is NO-GO. In
both outcomes `CompletionMarkers` remains `0`.

Append the exact process/module hash, trace hash/size, accepted candidate fields,
callback-order evidence, user observations, and decision to the report. Commit
and push the evidence-only documentation.

- [ ] **Step 5: Hand the GO evidence to the Phase 5B plan**

If GO, invoke `writing-plans` again and create
`docs/superpowers/plans/2026-07-14-fixed-map-completion-marker-rendering.md`.
That plan must use the accepted numeric row signature and observed callback order
from the report, then implement geometry, GDI frame drawing, committed-worker
publication, `CompletionMarkers=1`, and immediate-refresh acceptance. If NO-GO,
return to brainstorming for a separately approved internal list-model adapter;
do not write rendering code from guessed values.
