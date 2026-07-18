# Phase 2.5 Settlers United Lua Map Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Win32 diagnostic ASI that uses public S4ModApi LuaOpen, MapInit, and Tick listeners to read `SU.Game.GetMapName()` and `SU.Game.GetMapNameRelativePath()` without any process patch.

**Architecture:** A testable Lua API adapter feeds a bounded read-only bridge. A generation-aware session state machine and coordinator associate the two validated SU values with the explicit fixed-map list epoch and MapInit. `S4Listeners` performs all Lua work on the game Tick callback; `DiagnosticRuntime` owns lifecycle and an exclusive allowlisted project trace.

**Tech Stack:** C++17, Win32 API, Lua 3.2 functions exported by S4ModApi 2.3.2, CMake 3.24+, MSVC v143 Win32 `/W4 /WX /permissive-`, PowerShell, GitHub Actions `windows-2022`.

## Global Constraints

- Work directly on `main`; no worktree or feature branch, as explicitly authorized.
- Follow RED/GREEN TDD. Push each RED and GREEN commit because local MSVC Win32 is unavailable.
- The 0.2.5 ASI target constructs no `CallPatch`, `JmpPatch`, `NopPatch`, MinHook, Detours, or other process patch.
- Do not compile or link `FixedMapLoadHook`, `HlibCallPatchBackend`, `HookSiteLayout`, or `MsvcX86WideString` into the 0.2.5 ASI target.
- Use only public S4ModApi listeners and public Lua 3.2 functions already exported through S4ModApi.
- Call Lua only from the public Tick callback, on the game thread.
- Never call `lua_setglobal`, `lua_rawsetglobal`, `lua_settable`, `lua_rawsettable`, `lua_dostring`, `lua_dofile`, or `lua_dobuffer` in production.
- Copy every Lua return before `lua_endblock`; retain no Lua pointer or object.
- Bound each copied value to 512 bytes, retry interval to 50 ms, attempt count to 64, and deadline to 5,000 ms.
- Packaged `CaptureTraceRoot` remains empty. Only the authorized deployed INI may point at the project runtime directory.
- Modify no game/SU file except authorized `CampaignCompletion*.asi`, the `CampaignCompletion` config directory, and guarded `Plugin_SU.zip` replacement.
- Preserve original archive SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.
- Never terminate the game or Settlers United. The user performs launch, navigation, responsiveness checks, and shutdown.
- Do not implement victory, completion persistence, or markers in this phase.

## File structure

- Create `src/identity/SuMapValue.h/.cpp`: statuses, bounded string validation, code-page conversion, and relative-path normalization.
- Create `src/lua/LuaApi.h/.cpp`: mockable read-only Lua 3.2 adapter.
- Create `src/lua/SuLuaMapBridge.h/.cpp`: `ILuaMapBridge` plus the production two-function reader.
- Create `src/identity/LuaMapSession.h/.cpp`: generation binding, retry bounds, raw result stability, and terminal session result.
- Create `src/identity/MapIdentityCoordinator.h/.cpp`: list attribution, MapInit association, value validation, and trace emission.
- Create `tests/SuMapValueTests.cpp`, `tests/SuLuaMapBridgeTests.cpp`, `tests/LuaMapSessionTests.cpp`, and `tests/MapIdentityCoordinatorTests.cpp`.
- Modify `src/runtime/S4Listeners.h/.cpp`: register and dispatch LuaOpen and Tick.
- Modify `src/runtime/DiagnosticRuntime.h/.cpp`: remove Hook ownership and wire the Lua identity pipeline.
- Modify `src/diagnostics/CaptureTrace.cpp` and `tests/CaptureTraceTests.cpp`: replace Phase 2.4 trace allowlist.
- Modify `config/CampaignCompletionDebug.ini` and `tests/RuntimePolicyTests.cpp`: version 0.2.5, `FixedMapLoadHook=0`, `HookCount=0`.
- Modify `CMakeLists.txt` and `tests/TestMain.cpp`: source/test membership.
- Create `tools/verify_no_process_patch.ps1`; modify `.github/workflows/build-debug-asi.yml`.
- Create `docs/research/phase-2-5-su-lua-map-identity-report.md` after live testing.

---

### Task 1: Validate SU map values

**Files:**
- Create: `tests/SuMapValueTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Create: `src/identity/SuMapValue.h`
- Create: `src/identity/SuMapValue.cpp`

**Interfaces:**
- Produces `SuMapReadStatus`, `ValidatedSuValue`, `ValidateSuMapName`, `ValidateSuRelativePath`, and `SuMapReadStatusName`.
- Later tasks consume UTF-8 display values and normalized wide relative paths; no later task revalidates raw Lua bytes.

- [ ] **Step 1: Add the RED contract**

Declare `RunSuMapValueTests()` in `tests/TestMain.cpp`, call it before session tests, and add the test file plus production source to `campaign_completion_tests`. The test must require:

```cpp
using campaign_completion::SuMapReadStatus;
using campaign_completion::ValidateSuMapName;
using campaign_completion::ValidateSuRelativePath;

Require(ValidateSuMapName("Aeneas").status == SuMapReadStatus::Success,
        "plain name accepted");
Require(ValidateSuMapName("").status == SuMapReadStatus::Empty,
        "empty name rejected");
Require(ValidateSuMapName(std::string(513u, 'a')).status ==
            SuMapReadStatus::TooLong,
        "513-byte name rejected");
Require(ValidateSuMapName("bad\nname").status ==
            SuMapReadStatus::ControlCharacter,
        "trace injection rejected");

const auto relative = ValidateSuRelativePath("Maps//Single/./Aeneas.map");
Require(relative.status == SuMapReadStatus::Success,
        "relative path accepted");
Require(relative.normalized == L"Maps\\Single\\Aeneas.map",
        "relative path normalized");
for (const auto* invalid : {"C:\\Maps\\Aeneas.map", "C:Aeneas.map",
                            "\\Aeneas.map", "\\\\server\\share\\a.map",
                            "\\\\?\\C:\\a.map", "Maps\\..\\a.map"}) {
    Require(ValidateSuRelativePath(invalid).status !=
                SuMapReadStatus::Success,
            "absolute or traversal path rejected");
}
```

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/SuMapValueTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify SU map value validation"
git push origin main
```

Expected: Win32 compile fails because `identity/SuMapValue.h` is absent.

- [ ] **Step 3: Implement the public value model**

Use this exact header shape:

```cpp
enum class SuMapReadStatus {
    Success, SuTableMissing, GameTableMissing, FunctionMissing, CallError,
    NonString, Empty, TooLong, ControlCharacter, EncodingFailure,
    AbsolutePath, PathTraversal, ValueConflict, StaleGeneration, Timeout
};

struct ValidatedSuValue {
    SuMapReadStatus status = SuMapReadStatus::Empty;
    std::string displayUtf8;
    std::wstring normalized;
    explicit operator bool() const noexcept {
        return status == SuMapReadStatus::Success;
    }
};

ValidatedSuValue ValidateSuMapName(std::string_view bytes);
ValidatedSuValue ValidateSuRelativePath(std::string_view bytes);
std::string_view SuMapReadStatusName(SuMapReadStatus status) noexcept;
```

Implementation rules: reject zero/over-512/control bytes before conversion; use `MultiByteToWideChar(CP_ACP, 0, ...)`, normalize separators and components without accessing disk, reject drive/root/UNC/device/`..`, and use `WideCharToMultiByte(CP_UTF8, ...)` for trace display.

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/identity/SuMapValue.h src/identity/SuMapValue.cpp CMakeLists.txt
git commit -m "feat: validate SU map identity values"
git push origin main
```

Require the complete workflow to pass.

---

### Task 2: Build the read-only Lua bridge

**Files:**
- Create: `tests/SuLuaMapBridgeTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Create: `src/lua/LuaApi.h`
- Create: `src/lua/LuaApi.cpp`
- Create: `src/lua/SuLuaMapBridge.h`
- Create: `src/lua/SuLuaMapBridge.cpp`

**Interfaces:**
- Produces `ILuaApi`, `S4LuaApi`, `LuaValueResult`, `LuaMapReadAttempt`, `ILuaMapBridge`, and `S4LuaMapBridge::Read()`.
- `LuaMapSession` consumes `ILuaMapBridge` and never calls Lua directly.

- [ ] **Step 1: Write RED bridge tests with a fake Lua API**

The public bridge contract is:

```cpp
struct LuaValueResult {
    SuMapReadStatus status = SuMapReadStatus::FunctionMissing;
    std::string bytes;
};
struct LuaMapReadAttempt {
    LuaValueResult name;
    LuaValueResult relative;
};
class ILuaMapBridge {
public:
    virtual ~ILuaMapBridge() = default;
    virtual LuaMapReadAttempt Read() noexcept = 0;
};
```

Tests must fake table/function/result objects and require: two independent zero-argument calls; exact lookup chain `SU -> Game -> function`; copied bytes survive block end; every begun block ends on success, missing table, missing function, call error, non-string, null string, and over-limit string; no Lua setter/script method exists on `ILuaApi`.

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/SuLuaMapBridgeTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify read-only SU Lua map bridge"
git push origin main
```

Expected: compile fails because `lua/SuLuaMapBridge.h` is absent.

- [ ] **Step 3: Implement the mockable adapter and reader**

`ILuaApi` exposes only:

```cpp
using LuaObject = unsigned int;
virtual void BeginBlock() noexcept = 0;
virtual void EndBlock() noexcept = 0;
virtual LuaObject GetGlobal(char* name) noexcept = 0;
virtual LuaObject GetField(LuaObject object, char* field) noexcept = 0;
virtual bool IsTable(LuaObject object) noexcept = 0;
virtual bool IsFunction(LuaObject object) noexcept = 0;
virtual bool CallFunction(LuaObject function) noexcept = 0;
virtual LuaObject GetResult(int index) noexcept = 0;
virtual bool IsString(LuaObject object) noexcept = 0;
virtual const char* GetString(LuaObject object) noexcept = 0;
virtual long StringLength(LuaObject object) noexcept = 0;
```

`S4LuaApi` wraps only `lua_beginblock`, `lua_endblock`, `lua_getglobal`,
`lua_getfield`, `lua_istable`, `lua_isfunction`, `lua_callfunction`,
`lua_getresult`, `lua_isstring`, `lua_getstring`, and `lua_strlen`.
`S4LuaMapBridge` uses mutable static field-name arrays required by the Lua 3.2
ABI and an RAII block guard. It copies at most 512 bytes while the block is
active and returns `TooLong` without allocation when `lua_strlen > 512`.

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/lua/LuaApi.h src/lua/LuaApi.cpp src/lua/SuLuaMapBridge.h src/lua/SuLuaMapBridge.cpp CMakeLists.txt
git commit -m "feat: add read-only SU Lua map bridge"
git push origin main
```

Require the complete workflow to pass.

---

### Task 3: Implement generation-aware bounded sessions

**Files:**
- Create: `tests/LuaMapSessionTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Create: `src/identity/LuaMapSession.h`
- Create: `src/identity/LuaMapSession.cpp`

**Interfaces:**
- Consumes `ILuaMapBridge::Read()`.
- Produces `LuaMapSession::ObserveLuaOpen`, `ObserveMapInit`, `ObserveTick`, `Disable`, and `LuaMapSessionResult`.

- [ ] **Step 1: Add RED state-machine tests**

Use a queue-backed fake bridge and require:

```cpp
LuaMapSession session;
session.ObserveLuaOpen(100u);
const auto id = session.ObserveMapInit(200u);
Require(id == 1u, "first map-init gets session one");
Require(!session.ObserveTick(false, 250u, bridge).has_value(),
        "menu tick never calls Lua");
Require(!session.ObserveTick(true, 249u, bridge).has_value(),
        "tick before interval is ignored");
const auto success = session.ObserveTick(true, 250u, bridge);
Require(success && success->terminal && success->name.bytes == "Aeneas",
        "in-game tick resolves both values");
Require(!session.ObserveTick(true, 300u, bridge).has_value(),
        "terminal session never rereads Lua");
```

Add cases for MapInit before first LuaOpen binding once, second LuaOpen making a bound session stale, replacement by a second MapInit, 50 ms spacing, 64-attempt cap, 5,000 ms deadline, transient missing objects, name-only timeout, identical repeated values, conflicting successful values, and Disable.

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/LuaMapSessionTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify bounded Lua map sessions"
git push origin main
```

Expected: compile fails because `identity/LuaMapSession.h` is absent.

- [ ] **Step 3: Implement the state machine**

Use constants:

```cpp
inline constexpr std::uint64_t kLuaRetryIntervalMs = 50u;
inline constexpr std::uint64_t kLuaSessionDeadlineMs = 5000u;
inline constexpr std::size_t kLuaMaximumAttempts = 64u;
```

Missing-table/function statuses remain retryable. Call/type/value failures are terminal. Cache the first successful raw value for each field; a different later successful value produces `ValueConflict`. Emit exactly one terminal result, then retain only terminal metadata until the next MapInit or Disable.

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/identity/LuaMapSession.h src/identity/LuaMapSession.cpp CMakeLists.txt
git commit -m "feat: add bounded Lua map sessions"
git push origin main
```

Require the complete workflow to pass twice.

---

### Task 4: Coordinate list, MapInit, validation, and trace

**Files:**
- Create: `tests/MapIdentityCoordinatorTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Create: `src/identity/MapIdentityCoordinator.h`
- Create: `src/identity/MapIdentityCoordinator.cpp`
- Modify: `tests/CaptureTraceTests.cpp`
- Modify: `src/diagnostics/CaptureTrace.cpp`

**Interfaces:**
- Consumes fixed-map list attribution and `LuaMapSession` terminal results.
- Produces `ObserveListKind`, `ObserveBack`, `ObserveLuaOpen`, `ObserveMapInit`, `ObserveTick`, and `Disable`.

- [ ] **Step 1: Write RED coordinator and trace assertions**

Construct with record and trace sinks:

```cpp
MapIdentityCoordinator coordinator(
    [&](std::string line) { records.push_back(std::move(line)); },
    [&](std::string line) { trace.push_back(std::move(line)); });
coordinator.ObserveListKind(FixedMapListKind::Single, 100u);
coordinator.ObserveLuaOpen(120u);
coordinator.ObserveMapInit(200u);
coordinator.ObserveTick(true, 250u, bridge);
Require(Contains(trace, "su-map-name=Aeneas"), "name traced");
Require(Contains(trace, "identity-association=confirmed"),
        "both values confirm identity");
```

Add no-list, name-only, invalid relative path, stale generation, conflict, back invalidation, and Disable cases. Update `CaptureTraceTests` to accept only the eight Phase 2.5 prefixes and reject all Phase 2.4 adapter prefixes, CR/LF, absolute drive/UNC values, and forbidden scope terms.

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/MapIdentityCoordinatorTests.cpp tests/CaptureTraceTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify SU Lua identity coordination"
git push origin main
```

Expected: compile fails because the coordinator is absent and old trace records remain accepted.

- [ ] **Step 3: Implement minimal coordinator and allowlist**

The coordinator validates cached raw bytes only after a session becomes terminal. It emits one `map-init-session=<id>`, status records, validated value records, and one association. No-list attribution can never become confirmed. `ObserveBack` and `Disable` clear list/session state. The trace prefix array becomes exactly:

```cpp
"lua-open-generation=", "map-init-session=", "su-map-name-status=",
"su-map-relative-status=", "su-map-name=", "su-map-relative=",
"identity-association=", "controlled-stop-flush="
```

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/identity/MapIdentityCoordinator.h src/identity/MapIdentityCoordinator.cpp src/diagnostics/CaptureTrace.cpp CMakeLists.txt
git commit -m "feat: coordinate SU Lua map identity"
git push origin main
```

Require the complete workflow to pass twice.

---

### Task 5: Integrate LuaOpen and Tick public listeners

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `tests/ListenerRemovalTests.cpp`

**Interfaces:**
- `S4Listeners::Start(S4API, Logger&, MapIdentityCoordinator&, ILuaMapBridge&)` registers 77 public handles.
- `OnLuaOpen()` updates generation only; `OnTick(...)` is the sole Lua read path.

- [ ] **Step 1: Add RED static and lifecycle requirements**

Require source text to contain `AddLuaOpenListener(&OnLuaOpen)` and `AddTickListener(&OnTick)`, require no bridge read in `OnLuaOpen` or `OnMapInit`, and require the bridge to appear only in the bounded `ObserveTick` path. Extend removal tests with 77 fake handles and exact reverse order/count `77/77/0`.

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/RuntimePolicyTests.cpp tests/ListenerRemovalTests.cpp
git commit -m "test: require public Lua identity listeners"
git push origin main
```

Expected: CTest fails because LuaOpen and Tick are not registered.

- [ ] **Step 3: Wire callbacks under the existing callback gate**

Add:

```cpp
static HRESULT S4HCALL OnLuaOpen();
static HRESULT S4HCALL OnTick(DWORD tick, BOOL hasEvent, BOOL delayed);
void ObserveLuaOpen();
void ObserveTick(BOOL delayed);
```

Register LuaOpen then Tick after the existing public listeners. `ObserveTick` returns immediately for delayed ticks, asks `api_->IsCurrentlyOnScreen(S4_SCREEN_INGAME)`, and calls the coordinator with `GetTickCount64()` and the bridge. Do not hold the existing UI logging mutex while entering Lua.

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/runtime/S4Listeners.h src/runtime/S4Listeners.cpp
git commit -m "feat: observe SU Lua identity on public ticks"
git push origin main
```

Require the complete workflow to pass twice.

---

### Task 6: Remove the process Hook from the 0.2.5 runtime target

**Files:**
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `CMakeLists.txt`
- Modify: `tests/RuntimePolicyTests.cpp`

**Interfaces:**
- Runtime owns `S4LuaApi`, `S4LuaMapBridge`, and `MapIdentityCoordinator`.
- Stop disables the coordinator, removes 77 listeners, releases S4ModApi, writes stop status, then closes trace/logger.

- [ ] **Step 1: Add RED zero-Hook policy**

Require the INI fields:

```text
Version=0.2.5
IdentitySource=SettlersUnitedLua
FixedMapLoadHook=0
HookCount=0
CodePatchBytes=0
LuaWrites=0
```

Require runtime source to contain `version=0.2.5` and `identity-mode=su-lua-read-only`, to omit every Hook member/start/stop token, and to disable the coordinator before `listeners_.Stop()` and close the trace afterward. Require the ASI source list in `CMakeLists.txt` to omit all four Hook-era production files.

- [ ] **Step 2: Commit and push RED**

```bash
git add tests/RuntimePolicyTests.cpp
git commit -m "test: require zero-Hook SU Lua runtime"
git push origin main
```

Expected: CTest fails on version, runtime ownership, and target membership.

- [ ] **Step 3: Switch runtime, target, and configuration**

Remove Hook headers/members/flags and all Hook install/rollback/stop code. Construct the coordinator with logger and trace sinks, start listeners with the production bridge, and reset the coordinator after clean stop. Remove Hook-era sources from the ASI target. Retain the historical Hook unit-test sources unchanged; deleting historical tests is outside this plan. Add the new identity/Lua sources to both ASI and test targets.

- [ ] **Step 4: Commit and push GREEN**

```bash
git add src/runtime/DiagnosticRuntime.h src/runtime/DiagnosticRuntime.cpp config/CampaignCompletionDebug.ini CMakeLists.txt
git commit -m "feat: switch diagnostic runtime to SU Lua identity"
git push origin main
```

Require the complete workflow to pass twice.

---

### Task 7: Replace the Hook ABI verifier with zero-patch policy

**Files:**
- Create: `tools/verify_no_process_patch.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Consumes built ASI plus source root.
- Produces an exit-zero proof of PE32 layout, required ASI export, zero Hook constructors/references in target sources, and no forbidden Lua write APIs.

- [ ] **Step 1: Add a failing CI mutation fixture**

Change the workflow to call the new verifier before it exists. Add two fixtures: one inserts `hlib::CallPatch` into a copied production source; the other inserts `lua_setglobal` into a copied `SuLuaMapBridge.cpp`. Each fixture must be rejected.

- [ ] **Step 2: Commit and push RED**

```bash
git add .github/workflows/build-debug-asi.yml
git commit -m "test: require zero-patch binary policy"
git push origin main
```

Expected: workflow fails because `verify_no_process_patch.ps1` is absent.

- [ ] **Step 3: Implement the verifier**

The script locates x86 `dumpbin`, verifies the ASI is PE32 and exports exactly one `CampaignCompletionDebugStop` control entry, reads `CMakeLists.txt` to obtain the diagnostic target block, rejects Hook-era file membership, recursively scans production target sources for patch frameworks/tokens, and scans `SuLuaMapBridge.cpp` for the seven forbidden Lua APIs. It prints:

```text
Verified PE32 diagnostic ASI, zero process patches, and read-only Lua bridge.
```

- [ ] **Step 4: Commit and push GREEN**

```bash
git add tools/verify_no_process_patch.ps1 .github/workflows/build-debug-asi.yml
git commit -m "ci: verify zero-patch SU Lua diagnostic"
git push origin main
```

Require two complete successful workflow attempts, including both rejection fixtures, CTest, packaging, PE `0x014c`, and artifact upload.

---

### Task 8: Freeze, guarded-deploy, and run the Single/Aeneas gate

**Files:**
- Create ignored: `dist/phase-2-5/download/`
- Create ignored: `dist/phase-2-5/package/`
- Create ignored: `dist/phase-2-5/runtime/`
- Create after live test: `docs/research/phase-2-5-su-lua-map-identity-report.md`

**Interfaces:**
- Consumes the successful GitHub Actions artifact.
- Produces one frozen artifact set, one exclusive live trace, and exactly one GO/NO-GO report conclusion.

- [ ] **Step 1: Freeze and verify the artifact**

Download the successful artifact, record Actions run/job/artifact IDs and digest, extract only the ASI/INI package, compute SHA-256 and sizes, require PE32, exact ZIP layout, version 0.2.5, empty packaged trace root, and zero Hook policy.

- [ ] **Step 2: Require user-controlled shutdown and guarded install**

Proceed only after read-only checks find no game/SU process. Verify the immutable backup hash. Ask the user to run the elevated guarded installer with the frozen ASI and paste its output. Require embedded ASI hash equality.

- [ ] **Step 3: Deploy only the authorized INI difference**

Create the project runtime directory. Copy the frozen INI to the authorized config path and change only `CaptureTraceRoot` to the project runtime directory. Prove the normalized diff contains exactly that line and record the deployed hash.

- [ ] **Step 4: Run one user-driven Single/Aeneas test**

Ask the user to launch normally, wait at main menu, enter Single Player Maps, explicitly click ID `2449`, select Aeneas, enter the map, remain there, and report. Read only the current game PID's project trace.

GO requires `su-map-name-status=success`, `su-map-relative-status=success`, validated nonempty values, and `identity-association=confirmed` in one session/generation. Any missing object, timeout, invalid value, conflict, stale generation, or no-list result is NO-GO with that exact boundary.

- [ ] **Step 5: Controlled stop and responsiveness**

Create only `CampaignCompletionDebug.stop`. Require listener stop `77/77/0`, `diagnostic runtime stopped`, and `controlled-stop-flush=success`. Ask the user to confirm normal response. Never terminate the process.

- [ ] **Step 6: Write, verify, commit, and push the report**

Include commit/CI/artifact/frozen/deployed hashes, backup readiness, trace filename and PID, generation/session/status/association, stop counts, and user response. Include no absolute map path, Lua pointer/object/stack data, or unrelated module/UI evidence. State exactly one:

```text
NO-GO: the SU Lua map identity boundary failed with the recorded reason.
GO: the explicit list epoch, MapInit, SU map name, SU relative path, and Lua generation agreed.
```

Then run:

```bash
git diff --check
git status --short
git add docs/research/phase-2-5-su-lua-map-identity-report.md
git commit -m "docs: report phase 2.5 SU Lua identity"
git push origin main
test "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)"
```
