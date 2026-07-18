# Fixed-Map Completion Marker Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render one non-interactive green check on every completed visible row in the three Single Player fixed-map lists, track scrolling and state changes safely, and publish newly committed victories without restarting the game.

**Architecture:** Replace the Phase 5A trace-only runtime path with an allocation-free fixed-row observer, a fixed-capacity frame command buffer, pure checked geometry, and a failure-contained DirectDraw/GDI renderer. Keep stable completion identity in `CompletionMarkerIndex`; GUI text is only a category-bounded presentation match, while `CompletionWorker` republishes the store snapshot only after a successful commit.

**Tech Stack:** C++17, Win32 bounded SEH and ACP conversion, S4ModApi 2.3.2 public GUI-element/UI-frame listeners, DirectDraw 7, GDI, CMake/CTest, MSVC Win32 `/W4 /WX`, PowerShell packaging and policy checks, GitHub Actions.

## Global Constraints

- Work directly on `main`, as explicitly requested; do not create a worktree or feature branch.
- The game and Settlers United are currently closed. Do not deploy or alter a game-directory file until the candidate is audited and the user again explicitly approves deployment.
- Preserve the authorization boundary: read/copy access is allowed; only project-owned `CampaignCompletion*.asi`, the project-owned `Plugins\CampaignCompletion` configuration directory, and the separately approved guarded `Plugin_SU.zip` replacement may be changed.
- Use public S4ModApi callbacks only. Add no internal menu-model hook, code patch, Lua write, game-data write, mouse-intercepting control, or `GameDefaultGameEndCheck` call.
- Preserve completion detection, event `609` admission, completion persistence, backup recovery, random-map persistence, online exclusion, and controlled-stop behavior even if marker rendering disables itself.
- Stable identity remains the normalized relative map path. Row text, row position, `valueLink`, and `textStyle` are never persistence keys.
- A successfully loaded read-only backup snapshot seeds the initial marker index exactly like a writable loaded snapshot; read-only mode still prevents new persistence writes.
- The accepted Phase 5A logical row signature is surface `800x600`, container `0`, x `115`, width `271`, height `30`, y slots `142 + 31*n` for `n=0..5`, text styles `1|2`, and slot links `2417..2422`. These values are association guards; drawing always uses the current observed rectangle and current callback pillarbox value.
- Accepted callback order is candidate GUI observation followed by page `4` UI-frame callback. Live pillarbox evidence was `274`; runtime must use the callback value and apply it exactly once, never hard-code `274`.
- A repeated callback for the same label and identical rectangle is deduplicated. The same completed label at two different visible rectangles makes that label ambiguous for the whole frame and draws no check.
- Every page-set change, tab change, consumed page-4 frame, invalid frame, or disable clears pending commands. No stale rectangle may survive navigation or scrolling.
- The successful GUI observation and rendering path performs no heap allocation, file I/O, JSON parsing, directory enumeration, or success logging.
- Acquire the DirectDraw DC at most once per non-empty relevant frame and release it before returning on every path.
- A rendering failure affects only marker rendering. Three consecutive backend failures disable the renderer for the process and emit one terminal error; no failure disables persistence.
- The Phase 5B package uses version `0.6.0`, `MarkerCalibration=0`, `CompletionDetection=1`, `CompletionStorage=1`, and `CompletionMarkers=1`.
- Produce and audit a Win32 Release PE32/i386 package. Keep the two-entry package layout unchanged.

---

## File structure

- `src/marker/CompletionMarkerIndex.h/.cpp`: add allocation-free category/name match status while retaining Phase 5A lookup tests.
- `src/completion/CompletionStore.h`: expose a thread-safe snapshot through `ICompletionStore` for committed publication.
- `src/completion/CompletionWorker.h/.cpp`: publish a replacement snapshot only after `Committed`.
- `src/marker/BoundedMenuText.h/.cpp`: fixed-buffer SEH copy, lossless ACP decode, and whitespace trim without callback allocation.
- `src/marker/FixedMapRowObserver.h/.cpp`: calibrated signature gate, unique category/name association, duplicate-row suppression, and fixed-capacity frame commands.
- `src/marker/CompletionMarkerGeometry.h/.cpp`: checked logical-to-destination conversion and check polyline construction.
- `src/marker/CompletionMarkerRenderer.h/.cpp`: backend abstraction, bounded frame draw, failure rate limit, and terminal disable policy.
- `src/marker/DirectDrawMarkerSurface.h/.cpp`: DirectDraw description/DC lifecycle plus precreated GDI pens and clipped outlined polylines.
- `src/runtime/S4Listeners.h/.cpp`: feed the observer and render the preceding observations only on the calibrated page-4 frame.
- `src/runtime/DiagnosticRuntime.h/.cpp`: own the index/observer/surface/renderer in dependency-safe order and wire committed publication.
- `config/CampaignCompletionDebug.ini`: version `0.6.0`, rendering mode, calibration off, markers on.
- `tests/CompletionMarkerIndexTests.cpp`, `tests/CompletionWorkerTests.cpp`: allocation-free lookup and commit-only refresh.
- `tests/BoundedMenuTextTests.cpp`: fixed-copy/decode/trim boundary coverage.
- `tests/FixedMapRowObserverTests.cpp`: signature, category, ambiguity, frame, scroll, and stale-state coverage.
- `tests/CompletionMarkerGeometryTests.cpp`: size, clip, pillarbox, overflow, and destination-surface coverage.
- `tests/CompletionMarkerRendererTests.cpp`: one-DC draw, empty frame, failure isolation, and disable coverage.
- `tests/RuntimePolicyTests.cpp`, `tests/TestMain.cpp`, `CMakeLists.txt`: runtime policy, source membership, and test registration.
- `.github/workflows/build-debug-asi.yml`, `tools/package_debug.ps1`, `tools/verify_no_process_patch.ps1`: Release candidate build/package/policy gates without changing the artifact name.
- `docs/research/phase-5b-fixed-map-completion-marker-report.md`: build, review, deployment, and live acceptance evidence.

---

### Task 1: Commit-only marker-index publication

**Files:**
- Modify: `src/marker/CompletionMarkerIndex.h`
- Modify: `src/marker/CompletionMarkerIndex.cpp`
- Modify: `src/completion/CompletionStore.h`
- Modify: `src/completion/CompletionStore.cpp`
- Modify: `src/completion/CompletionWorker.h`
- Modify: `src/completion/CompletionWorker.cpp`
- Modify: `tests/CompletionMarkerIndexTests.cpp`
- Modify: `tests/CompletionWorkerTests.cpp`

**Interfaces:**
- Consumes: `CompletionDatabaseSnapshot`, `CompletionAddStatus`, and the existing fixed/category filtering rules.
- Produces: `MarkerMatchStatus CompletionMarkerIndex::Match(FixedMapListKind, std::wstring_view) const noexcept`; `ICompletionStore::Snapshot() const`; and `CompletionWorker::SnapshotSink` invoked only after a committed transaction.

- [ ] **Step 1: Add the failing allocation-free match and commit-publication tests**

Add to `CompletionMarkerIndexTests.cpp`:

```cpp
Require(index.Match(FixedMapListKind::Single, L"Aeneas") ==
            MarkerMatchStatus::Unique,
        "one category-correct record must match uniquely");
Require(index.Match(FixedMapListKind::Custom, L"Aeneas") ==
            MarkerMatchStatus::None,
        "the same name in a wrong category must not match");
Require(index.Match(FixedMapListKind::Unknown, L"Aeneas") ==
            MarkerMatchStatus::None,
        "unknown category must fail closed");
index.Publish(DuplicateAntaresSnapshot());
Require(index.Match(FixedMapListKind::Custom, L"Antares") ==
            MarkerMatchStatus::Ambiguous,
        "two indexed candidates with one display name must be ambiguous");
```

Extend the controllable worker store so it owns a `CompletionDatabaseSnapshot`
and implements `Snapshot() const`. Add four isolated worker cases:

```cpp
std::vector<CompletionDatabaseSnapshot> publications;
CompletionWorker worker(
    store,
    [&logs](LogLevel level, std::string line) {
        logs.emplace_back(level, std::move(line));
    },
    [&publications](CompletionDatabaseSnapshot snapshot) {
        publications.push_back(std::move(snapshot));
    });
```

Require one publication containing the committed record after `Committed`, and
zero publications after `Duplicate`, `ReadOnly`, and `Failed`. Add a throwing
snapshot sink and require the committed store result to remain committed, the
worker to drain, and one `completion-marker-index publish-failed` error log.

- [ ] **Step 2: Commit RED and verify the expected CI compile failure**

```bash
git add tests/CompletionMarkerIndexTests.cpp tests/CompletionWorkerTests.cpp
git commit -m "test: require commit-only marker refresh"
git push origin main
gh.exe run watch --exit-status
```

Expected: Build fails because `MarkerMatchStatus`, `Match`, the virtual snapshot
interface, and the three-argument worker constructor do not exist. Archive and
mutation fixtures must not be the failure source.

- [ ] **Step 3: Implement minimal match and publication behavior**

Declare:

```cpp
enum class MarkerMatchStatus {
    None,
    Unique,
    Ambiguous,
};

class CompletionMarkerIndex final {
public:
    void Publish(const CompletionDatabaseSnapshot& snapshot) noexcept;
    MarkerCandidateSnapshot Find(FixedMapListKind listKind,
                                 std::wstring_view rowLabel) const noexcept;
    MarkerMatchStatus Match(FixedMapListKind listKind,
                            std::wstring_view rowLabel) const noexcept;
};
```

`Match` scans the already filtered vector under the existing mutex, uses the
same ordinal case-insensitive comparison as `Find`, returns immediately on the
second match, and performs no vector copy or allocation.

Add to `ICompletionStore` and mark the existing concrete method `override`:

```cpp
virtual CompletionDatabaseSnapshot Snapshot() const = 0;
```

Change the worker constructor and members to:

```cpp
using SnapshotSink =
    std::function<void(CompletionDatabaseSnapshot snapshot)>;

CompletionWorker(ICompletionStore& store, LogSink log,
                 SnapshotSink publish);

SnapshotSink publish_;
```

Immediately after `AddIfAbsent` returns `Committed`, call `store_.Snapshot()`
and then `publish_(std::move(snapshot))` inside one exception boundary. Do not
call it for any other status. A publication exception logs exactly
`completion-marker-index publish-failed` and does not change the store result,
queue accounting, or drain behavior. Existing constructors in runtime/tests
must pass an empty third callback until Task 5 wires the real index.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

```bash
git add src/marker/CompletionMarkerIndex.h \
  src/marker/CompletionMarkerIndex.cpp \
  src/completion/CompletionStore.h src/completion/CompletionStore.cpp \
  src/completion/CompletionWorker.h src/completion/CompletionWorker.cpp \
  tests/CompletionMarkerIndexTests.cpp tests/CompletionWorkerTests.cpp \
  src/runtime/DiagnosticRuntime.cpp
git commit -m "feat: publish marker index after committed victory"
git push origin main
gh.exe run watch --exit-status
```

Expected: all workflow gates pass. Store snapshot publication is observable
only after `Committed`, and existing persistence tests remain green.

---

### Task 2: Allocation-free fixed-row observer

**Files:**
- Create: `src/marker/BoundedMenuText.h`
- Create: `src/marker/BoundedMenuText.cpp`
- Create: `src/marker/FixedMapRowObserver.h`
- Create: `src/marker/FixedMapRowObserver.cpp`
- Create: `tests/BoundedMenuTextTests.cpp`
- Create: `tests/FixedMapRowObserverTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: copied public GUI element scalars, exact `PageSnapshot`, approved `FixedMapListKind`, and `CompletionMarkerIndex::Match`.
- Produces: stack-only `BoundedMenuText`, `BoundedWideText`, `FixedMapRowObservation`, `MarkerDrawCommand`, `MarkerFrameCommands`, and `FixedMapRowObserver::{ObservePages, ObserveListKind, ObserveElement, TakeFrame, Disable}`.

- [ ] **Step 1: Add failing fixed-buffer and observer tests**

Define tests against these exact public types:

```cpp
inline constexpr std::size_t kMaximumMarkerTextUnits = 256u;
inline constexpr std::size_t kMaximumVisibleFixedRows = 6u;

struct BoundedMenuText final {
    std::array<char, kMaximumMarkerTextUnits + 1u> bytes{};
    std::uint16_t length = 0u;
};

struct BoundedWideText final {
    std::array<wchar_t, kMaximumMarkerTextUnits + 1u> units{};
    std::uint16_t length = 0u;
    std::wstring_view view() const noexcept {
        return {units.data(), length};
    }
};
```

Test `CopyBoundedGuiText` with null, `"Aeneas"`, 255 bytes plus NUL, and
256 non-NUL bytes. Test lossless ACP decoding, leading/trailing ASCII spaces,
embedded control rejection, and no output mutation on failure.

Use a real `CompletionMarkerIndex` with Aeneas/Antares and require:

- exact pages `{4,22,23,25}` plus Single accepts only the calibrated Aeneas row;
- Custom accepts only Antares; Multiplayer returns no command;
- surface other than `800x600`, container other than `0`, x other than `115`,
  width/height other than `271x30`, a y not equal to `142 + 31*n`, a link not
  equal to `2417+n`, and a text style outside `1|2` each fail closed;
- spaces around `Aeneas` are trimmed for presentation matching;
- repeated identical callbacks collapse to one command;
- the same label at two different valid rectangles clears that label and yields
  no command for the frame;
- two different completed labels at the same valid rectangle invalidate and
  drop the whole frame; the six-row capacity guard independently fails closed
  if its counter is ever exhausted;
- only `TakeFrame(4u)` consumes commands; a non-page-4 callback returns empty
  without consuming the pending page-4 data;
- every consumed frame, page change, tab change, and `Disable()` clears state;
- returned commands carry the current rectangle and logical surface only, not
  `valueLink`, text style, text, or a raw pointer.

- [ ] **Step 2: Commit RED and verify missing-interface failure**

```bash
git add CMakeLists.txt tests/TestMain.cpp \
  tests/BoundedMenuTextTests.cpp tests/FixedMapRowObserverTests.cpp
git commit -m "test: require allocation-free fixed-row observer"
git push origin main
gh.exe run watch --exit-status
```

Expected: compilation fails on missing bounded-text and observer headers.

- [ ] **Step 3: Implement bounded text and calibrated observation**

Declare the transport and output types:

```cpp
struct FixedMapRowObservation final {
    DWORD surfaceWidth = 0u;
    DWORD surfaceHeight = 0u;
    WORD containerType = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
    WORD valueLink = 0u;
    WORD textStyle = 0u;
    BoundedMenuText text{};
};

struct MarkerDrawCommand final {
    DWORD logicalSurfaceWidth = 0u;
    DWORD logicalSurfaceHeight = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
};

struct MarkerFrameCommands final {
    std::array<MarkerDrawCommand, kMaximumVisibleFixedRows> commands{};
    std::size_t count = 0u;
    std::uint64_t generation = 0u;
};
```

Implement the byte copy in a dedicated MSVC SEH helper that owns no C++ object
inside `__try/__except`. Decode with `MultiByteToWideChar(CP_ACP,
MB_PRECOMPOSED, ...)`, round-trip with `WC_NO_BEST_FIT_CHARS`, and use
`GetStringTypeW(CT_CTYPE1, ...) & C1_SPACE` only to trim leading/trailing
whitespace. Reject controls, empty trimmed labels, loss, and overlength text.

Declare the observer:

```cpp
class FixedMapRowObserver final {
public:
    explicit FixedMapRowObserver(const CompletionMarkerIndex& index);
    void ObservePages(const PageSnapshot& snapshot) noexcept;
    void ObserveListKind(FixedMapListKind kind) noexcept;
    void ObserveElement(const FixedMapRowObservation& element) noexcept;
    MarkerFrameCommands TakeFrame(DWORD page) noexcept;
    void Disable() noexcept;
};
```

Use only fixed arrays and counters internally. Require the exact calibrated
signature, but use `valueLink` only to validate the visible slot; never retain
it in a command. Match the trimmed label with `index_.Match`. Collapse an exact
same-label/same-rectangle repeat. Mark a same-label/different-rectangle pair
ambiguous and suppress it for the consumed frame. Two different labels cannot
occupy the same calibrated slot; reject the whole frame if they do. On any
capacity overflow, clear and invalidate the whole frame.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

```bash
git add CMakeLists.txt src/marker/BoundedMenuText.h \
  src/marker/BoundedMenuText.cpp src/marker/FixedMapRowObserver.h \
  src/marker/FixedMapRowObserver.cpp tests/BoundedMenuTextTests.cpp \
  tests/FixedMapRowObserverTests.cpp tests/TestMain.cpp
git commit -m "feat: observe calibrated fixed-map rows"
git push origin main
gh.exe run watch --exit-status
```

Expected: all tests pass; the observer contains no `std::string`,
`std::wstring`, `std::vector`, filesystem call, logger, or raw callback pointer.

---

### Task 3: Checked check-mark geometry

**Files:**
- Create: `src/marker/CompletionMarkerGeometry.h`
- Create: `src/marker/CompletionMarkerGeometry.cpp`
- Create: `tests/CompletionMarkerGeometryTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: one `MarkerDrawCommand`, callback pillarbox width, and actual destination-surface extent.
- Produces: `std::optional<MarkerCheckGeometry> BuildMarkerCheckGeometry(...) noexcept` containing a clipped three-point check in destination coordinates.

- [ ] **Step 1: Add failing geometry tests**

Declare and test:

```cpp
struct MarkerCheckGeometry final {
    RECT clip{};
    std::array<POINT, 3u> points{};
};

std::optional<MarkerCheckGeometry> BuildMarkerCheckGeometry(
    const MarkerDrawCommand& row,
    INT32 pillarboxWidth,
    DWORD destinationWidth,
    DWORD destinationHeight) noexcept;
```

For the live Aeneas row `115,142,271,30`, logical `800x600`, pillarbox `274`,
and destination `1348x600`, require:

```cpp
Require(geometry->clip.left == 389 && geometry->clip.right == 660,
        "pillarbox must shift the logical row exactly once");
Require(geometry->clip.top == 142 && geometry->clip.bottom == 172,
        "vertical coordinates remain logical surface coordinates");
Require(geometry->points[2].x == 656,
        "check right edge is inset four pixels from the row");
```

Require size `15` for height `30`, size `12` at the lower clamp, size `16` at
the upper clamp, all points inside the row clip, and rejection for negative
pillarbox, zero/small rows, overflow, logical out-of-bounds rows, destination
height mismatch, and destination width unequal to
`logicalSurfaceWidth + 2*pillarboxWidth`.

- [ ] **Step 2: Commit RED and verify missing geometry failure**

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/CompletionMarkerGeometryTests.cpp
git commit -m "test: require checked completion-marker geometry"
git push origin main
gh.exe run watch --exit-status
```

Expected: compilation fails because the geometry interface is absent.

- [ ] **Step 3: Implement pure bounded geometry**

Compute `size = clamp(row.height / 2, 12, 16)`, reject a row unable to contain
the check plus four-pixel right inset, and build:

```cpp
const LONG right = shiftedRowRight - 4;
const LONG left = right - size;
const LONG top = shiftedRowTop +
                  (static_cast<LONG>(row.height) - size) / 2;
geometry.points = {{
    {left, top + size / 2},
    {left + size / 3, top + size},
    {right, top},
}};
```

Use checked 64-bit intermediates for every sum and conversion. The clip is the
current row rectangle after adding the callback pillarbox once. Perform no GDI
call and no allocation.

- [ ] **Step 4: Run tests and commit GREEN**

```bash
git add CMakeLists.txt src/marker/CompletionMarkerGeometry.h \
  src/marker/CompletionMarkerGeometry.cpp \
  tests/CompletionMarkerGeometryTests.cpp tests/TestMain.cpp
git commit -m "feat: compute clipped marker geometry"
git push origin main
gh.exe run watch --exit-status
```

Expected: all geometry and existing calibration regressions pass.

---

### Task 4: Failure-contained DirectDraw/GDI renderer

**Files:**
- Create: `src/marker/CompletionMarkerRenderer.h`
- Create: `src/marker/CompletionMarkerRenderer.cpp`
- Create: `src/marker/DirectDrawMarkerSurface.h`
- Create: `src/marker/DirectDrawMarkerSurface.cpp`
- Create: `tests/CompletionMarkerRendererTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MarkerFrameCommands`, current `LPDIRECTDRAWSURFACE7`, pillarbox width, and monotonic milliseconds.
- Produces: one bounded outlined-check draw pass and renderer-only disable status; no mouse/control behavior.

- [ ] **Step 1: Add failing renderer/backend-contract tests**

Declare:

```cpp
struct MarkerSurfaceExtent final {
    DWORD width = 0u;
    DWORD height = 0u;
};

class IMarkerDrawingSurface {
public:
    virtual ~IMarkerDrawingSurface() = default;
    virtual bool Describe(LPDIRECTDRAWSURFACE7 surface,
                          MarkerSurfaceExtent& extent) noexcept = 0;
    virtual bool Begin(LPDIRECTDRAWSURFACE7 surface) noexcept = 0;
    virtual bool DrawOutlinedCheck(
        const MarkerCheckGeometry& geometry) noexcept = 0;
    virtual bool End() noexcept = 0;
};

enum class MarkerRenderStatus {
    Skipped,
    Drawn,
    Failed,
    Disabled,
};
```

Use a fake backend with call counters. Require an empty frame to make zero
backend calls; two valid commands to call `Describe` once, `Begin` once,
`DrawOutlinedCheck` twice, and `End` once; invalid geometry to drop the entire
frame before `Begin`; a draw failure still to call `End`; a later success to
reset the consecutive failure count; and three consecutive Begin/Draw/End
failures to produce one terminal disable log and no later backend calls.

Pass `nowMs` values `0`, `1000`, and `6000` and require transient
`completion-marker-renderer frame-failed` logs no more often than once per
5000 ms. Require no log on a successful or empty frame.

- [ ] **Step 2: Commit RED and verify missing renderer failure**

```bash
git add CMakeLists.txt tests/TestMain.cpp \
  tests/CompletionMarkerRendererTests.cpp
git commit -m "test: require failure-contained marker renderer"
git push origin main
gh.exe run watch --exit-status
```

Expected: compilation fails on missing renderer/surface interfaces.

- [ ] **Step 3: Implement renderer and real surface backend**

Declare the renderer:

```cpp
class CompletionMarkerRenderer final {
public:
    using LogSink = std::function<void(LogLevel, std::string)>;

    CompletionMarkerRenderer(IMarkerDrawingSurface& surface, LogSink log);
    MarkerRenderStatus Render(LPDIRECTDRAWSURFACE7 destination,
                              INT32 pillarboxWidth,
                              const MarkerFrameCommands& frame,
                              std::uint64_t nowMs) noexcept;
    void Disable() noexcept;
};
```

Build every geometry into a fixed six-element stack array after one `Describe`.
If any command fails geometry, reject the whole frame before `Begin`. Once
begun, draw all checks, always call `End`, and count the frame failed if any
draw or `End` fails. Three consecutive failed frames set a terminal disabled
flag and emit exactly `completion-marker-renderer disabled failures=3`.

`DirectDrawMarkerSurface::Initialize()` creates two process-lifetime pens before
listeners start: dark `RGB(24,40,24)` width `3`, green `RGB(48,208,80)` width
`1`. `Describe` calls `GetSurfaceDesc`; `Begin` calls `GetDC` once;
`DrawOutlinedCheck` uses `SaveDC`, `IntersectClipRect`, dark `Polyline`, green
`Polyline`, and `RestoreDC`; `End` calls `ReleaseDC`. Restore selected objects,
release the DC on all paths, delete pens only after callbacks quiesce, and catch
every exception at the backend boundary.

- [ ] **Step 4: Run focused/full tests and commit GREEN**

```bash
git add CMakeLists.txt src/marker/CompletionMarkerRenderer.h \
  src/marker/CompletionMarkerRenderer.cpp \
  src/marker/DirectDrawMarkerSurface.h \
  src/marker/DirectDrawMarkerSurface.cpp \
  tests/CompletionMarkerRendererTests.cpp tests/TestMain.cpp
git commit -m "feat: render bounded completion checks"
git push origin main
gh.exe run watch --exit-status
```

Expected: all tests pass with `Gdi32` linked; no renderer test requires a real
DirectDraw surface.

---

### Task 5: Runtime integration and Release package

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `src/completion/CompletionRecord.h`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `tests/CompletionRecordTests.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/build-debug-asi.yml`
- Modify: `tools/package_debug.ps1`
- Modify: `tools/verify_no_process_patch.ps1`

**Interfaces:**
- Consumes: committed index snapshots, copied fixed-row observations, page/tab state, public page-4 surface/pillarbox callback, and renderer status.
- Produces: version `0.6.0` Release x86 package with active markers, inactive calibration trace, safe shutdown, and unchanged persistence semantics.

- [ ] **Step 1: Add failing runtime/configuration/policy assertions**

Require the INI exact fields:

```text
Version=0.6.0
DiagnosticMode=CompletionMarkerRendering
MarkerCalibration=0
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=1
```

Require `kCompletionPluginVersion == "0.6.0"`. Require runtime ownership in
dependency-safe order:

```cpp
std::unique_ptr<CompletionMarkerIndex> markerIndex_;
std::unique_ptr<FixedMapRowObserver> markerObserver_;
std::unique_ptr<DirectDrawMarkerSurface> markerSurface_;
std::unique_ptr<CompletionMarkerRenderer> markerRenderer_;
std::unique_ptr<CompletionWorker> worker_;
```

Require initial `markerIndex_->Publish(store_->Snapshot())` after successful
load, including `CompletionStoreMode::ReadOnlyBackup`, and the worker snapshot
callback to publish into the same index. Require
observer and renderer disable before `listeners_.Stop()`, worker drain before
index destruction, and surface destruction after renderer destruction.

Require listeners to use fixed-buffer `CopyBoundedGuiText`, feed pages/tab/GUI
elements to `FixedMapRowObserver`, call `TakeFrame(page)`, and pass the returned
frame with the same callback surface/pillarbox to `Render`. Forbid retained raw
GUI pointers, `CreateCustomUiElement`, `AddGuiBltListener`, marker trace open,
calibration runtime ownership, internal hook tokens, per-frame logger calls,
JSON/filesystem calls in marker sources, and drawing calls outside
`DirectDrawMarkerSurface.cpp`.

Require the ASI target to link observer/geometry/renderer/surface but not
`MarkerCalibrationTrace.cpp` or `FixedMapRowCalibration.cpp`; tests continue to
link Phase 5A sources as regressions. Require `Gdi32`. Require workflow/package
paths to use `build/Release`, CTest `-C Release`, and the unchanged artifact
name `CampaignCompletionDebug-Win32`.

- [ ] **Step 2: Commit RED and verify policy failure**

```bash
git add tests/CompletionRecordTests.cpp tests/RuntimePolicyTests.cpp
git commit -m "test: require fixed-map completion rendering runtime"
git push origin main
gh.exe run watch --exit-status
```

Expected: Build succeeds against the Phase 5A runtime, then CTest fails on
version, ownership, rendering, configuration, and Release-policy assertions.

- [ ] **Step 3: Wire observer, renderer, and committed refresh**

Construct runtime dependencies after the successful store load:

```cpp
markerIndex_ = std::make_unique<CompletionMarkerIndex>();
markerIndex_->Publish(store_->Snapshot());
markerObserver_ =
    std::make_unique<FixedMapRowObserver>(*markerIndex_);
markerSurface_ = std::make_unique<DirectDrawMarkerSurface>();
markerRenderer_ = std::make_unique<CompletionMarkerRenderer>(
    *markerSurface_, [this](LogLevel level, std::string line) {
        logger_.Write(level, line);
    });
if (!markerSurface_->Initialize()) {
    markerRenderer_->Disable();
    logger_.Write(LogLevel::Error,
                  "completion-marker-renderer initialization-failed");
}
worker_ = std::make_unique<CompletionWorker>(
    *store_,
    [this](LogLevel level, std::string line) {
        logger_.Write(level, line);
    },
    [this](CompletionDatabaseSnapshot snapshot) {
        if (markerIndex_ != nullptr) {
            markerIndex_->Publish(snapshot);
        }
    });
```

Change `S4Listeners::Start` to receive observer and renderer references. In the
GUI callback, copy documented scalar fields plus text into
`FixedMapRowObservation` and immediately discard the S4ModApi pointer. In the
page-4 UI callback, consume the observer before publishing the next page-window
state:

```cpp
const auto frame = markerObserver_->TakeFrame(page);
markerRenderer_->Render(surface, pillarboxWidth, frame, now);
```

Then continue existing origin/settlement/page observation unchanged. Feed each
new `PageSnapshot` and approved tab attribution into the observer. Rendering
must not consume mouse events or suppress native UI callbacks.

During controlled stop and abort, disable admission, observer, and renderer,
then close/wait/remove listeners, drain the worker, reset worker, renderer,
surface, observer, index, store, and remaining resources in that order.

Set banner/config/record version to `0.6.0`, mode to
`completion-marker-rendering`, calibration to `0`, and markers to `1`. Remove
Phase 5A trace construction from runtime. Update the workflow and package script
to build/test/package Release while retaining PE machine `0x014c` and the exact
two-entry ZIP.

Do not require a writable store to construct the index: every successful load
mode publishes `store_->Snapshot()`, so a valid read-only backup displays its
existing completed rows while `CompletionWorker` continues to respect the
store's read-only result for new submissions.

- [ ] **Step 4: Run all workflow gates and commit GREEN**

```bash
git add CMakeLists.txt config/CampaignCompletionDebug.ini \
  src/completion/CompletionRecord.h src/runtime/DiagnosticRuntime.h \
  src/runtime/DiagnosticRuntime.cpp src/runtime/S4Listeners.h \
  src/runtime/S4Listeners.cpp tests/CompletionRecordTests.cpp \
  tests/RuntimePolicyTests.cpp .github/workflows/build-debug-asi.yml \
  tools/package_debug.ps1 tools/verify_no_process_patch.ps1
git commit -m "feat: integrate fixed-map completion markers"
git push origin main
gh.exe run watch --exit-status
```

Expected: archive integration, Release Win32 `/W4 /WX` Build, process-mutation
policy, CTest, package, PE32/layout verification, and artifact upload all pass.
The packaged INI has markers on and calibration off.

---

### Task 6: Review, artifact audit, guarded deployment, and live acceptance

**Files:**
- Create: `docs/research/phase-5b-fixed-map-completion-marker-report.md`
- Use without tracking: `artifacts/phase-5b-completion-markers/`
- Modify after live evidence: `docs/research/phase-5b-fixed-map-completion-marker-report.md`

**Interfaces:**
- Consumes: successful Release artifact, reviewer findings, explicit user deployment approval, closed processes, guarded installer, and user-driven UI/victory actions.
- Produces: a verified deployed marker build and an evidence-backed live GO/NO-GO, including same-process immediate refresh.

- [ ] **Step 1: Perform code review and fix every accepted finding**

Invoke `requesting-code-review`. Review the full Phase 5B implementation range
for wrong-row risk, stale frame state, duplicate rows, DC/pen leaks, exception
escape, shutdown races, publication before commit, per-frame allocations/I/O,
and persistence regression. For each real defect, use systematic debugging and
TDD: add a failing regression, verify RED, implement one fix, and rerun all CI
gates. Record the reviewed commit range and resolutions in the report.

- [ ] **Step 2: Download and audit the exact successful Release artifact**

```bash
run_id="$(gh.exe run list --workflow build-debug-asi.yml --branch main \
  --status success --limit 1 --json databaseId --jq '.[0].databaseId')"
mkdir -p "artifacts/phase-5b-completion-markers/run-$run_id"
gh.exe run download "$run_id" --name CampaignCompletionDebug-Win32 \
  --dir "artifacts/phase-5b-completion-markers/run-$run_id"
sha256sum "artifacts/phase-5b-completion-markers/run-$run_id/"*.zip
unzip -l "artifacts/phase-5b-completion-markers/run-$run_id/"*.zip
```

Extract below the same untracked run directory. Record run/job/artifact IDs and
digest, ZIP/ASI/INI sizes and SHA-256, PE machine `0x014c`, exact package entries,
Release configuration, and INI policy. Rerun the temporary SU integration test
locally and use the successful CI policy verifier when local `dumpbin` remains
unavailable.

- [ ] **Step 3: Commit the build report and stop for deployment approval**

Create the report with `apply_patch`, then:

```bash
git add docs/research/phase-5b-fixed-map-completion-marker-report.md
git commit -m "docs: verify fixed-map completion marker build"
git push origin main
```

Report exact candidate hashes. Ask the user to close both processes and
explicitly approve deployment. Do not install before that response.

- [ ] **Step 4: Deploy only the audited project artifacts**

After approval, verify the current installed archive equals the prior guarded
metadata hash, the immutable original remains
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`,
and neither process runs. Back up the entire live project configuration
directory to a new project-owned pre-v0.6.0 directory and verify every copied
file hash. Run only the existing guarded archive installer with the audited
ASI, atomically replace only `CampaignCompletionDebug.ini`, and independently
verify all protected ZIP entries, embedded ASI, live INI, database hash, and
absence of temporary siblings before asking the user to launch.

- [ ] **Step 5: Run visible marker/navigation acceptance one action at a time**

Ask the user to launch and then perform, with a responsiveness confirmation
after each group:

1. Single Player Maps: Aeneas normal, hover, selected, single-click,
   double-click-cancel/back, scroll out, and scroll back;
2. Custom Maps: the same Antares states and scrolling;
3. Multiplayer Maps: open and scroll as the zero-marker control;
4. Back navigation and one ordinary fixed-map launch;
5. normal return and normal exit.

Read only the project log and database between groups. Require exactly one
visible check on each target row, current-row tracking with no drift/stale mark,
no check in Multiplayer Maps, unchanged input behavior, no renderer warning or
terminal disable, and normal exit.

- [ ] **Step 6: Run same-process immediate-refresh acceptance**

In a fresh process, record the database hash/count and choose one eligible
previously unrecorded fixed map from Single Player Maps or Custom Maps. Ask the
user to launch it and obtain a real local-player event `609` victory through the
already approved AI Resign test flow, then return to the correct fixed-map list
without restarting the game. Require:

- one `CompletionAddStatus::Committed` log for the new stable ID;
- database count increases by exactly one and preserves all earlier records;
- the new row displays exactly one check on the first relevant list frame;
- no JSON reload, process restart, wrong-row change, duplicate record, or marker
  renderer error;
- random-map and online exclusion behavior remains unchanged.

- [ ] **Step 7: Apply the final GO/NO-GO and commit evidence**

GO requires every static, deployment, navigation, rendering, scrolling, input,
shutdown, and immediate-refresh gate above. Any wrong-row check, stale geometry,
DC/resource failure, input regression, premature publication, persistence
change outside the one committed record, renderer disable, or exit error is
NO-GO.

On GO, append exact loaded module/log/database identities and user observations,
commit, and push the report. On NO-GO, have the user close the game, obtain
explicit rollback approval, redeploy the audited Phase 5A ASI/INI through the
same guarded paths so `CompletionMarkers=0`, and document the failure without
weakening completion detection or storage.

---

## Plan completion boundary

This plan ends only after Phase 5B live acceptance, including same-process
immediate refresh, has an evidence-backed GO or the marker feature has been
safely returned to `CompletionMarkers=0`. Campaign markers, random-map UI,
manual completion editing, configurable artwork, and any internal list-model
adapter remain outside scope.
