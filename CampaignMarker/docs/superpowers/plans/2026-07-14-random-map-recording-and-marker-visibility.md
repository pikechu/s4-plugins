# Random-Map Recording and Marker Visibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Classify fresh and loaded random maps from validated SU identity, keep their victories recordable, and expose a separate policy that suppresses their future completion marker.

**Architecture:** Add a pure map-session policy module that mirrors the game's bounded random-identifier rule and refines post-`MapInit` launch origin without an internal game call. Make the identity coordinator return only a confirmed, session-bound identity observation, then let `S4Listeners` refine the active origin and emit a bounded trace. Online multiplayer remains dominant; persistence and UI rendering stay disabled in this phase.

**Tech Stack:** C++17, Win32/x86 MSVC, S4ModApi public listeners, Settlers United Lua API, CMake/CTest.

## Global Constraints

- Work directly on `main`; do not create a worktree or feature branch.
- Never modify or delete game-directory files during implementation or verification.
- Do not deploy while the game process is running.
- Do not add a game executable call, process-memory probe, new hook, or save-file parser.
- Random-map victories are recordable; random-map completion markers are hidden.
- True online multiplayer remains excluded from victory recording.
- Completion persistence and marker rendering remain disabled.

---

## File Structure

- Create `src/victory/MapSessionPolicy.h/.cpp`: pure identifier classification, origin refinement, recording policy, and marker-visibility policy.
- Create `tests/MapSessionPolicyTests.cpp`: table-driven boundary and policy tests.
- Modify `src/victory/LaunchOrigin.h/.cpp`: make fresh random maps eligible and add a resolved ordinary-load source.
- Modify `src/identity/MapIdentityCoordinator.h/.cpp`: return a confirmed session-bound identity observation.
- Modify `tests/MapIdentityCoordinatorTests.cpp`: verify confirmed results are returned and unsafe results are not.
- Modify `src/runtime/S4Listeners.cpp`: apply identity refinement to the active session and trace the result.
- Modify `src/diagnostics/Phase3Trace.cpp` and `tests/Phase3TraceTests.cpp`: admit only the new bounded origin-refinement record.
- Modify `CMakeLists.txt` and `tests/TestMain.cpp`: compile and run the new policy module tests.
- Modify `config/CampaignCompletionDebug.ini`, `src/runtime/DiagnosticRuntime.cpp`, and `tests/RuntimePolicyTests.cpp`: identify the changed diagnostic build as version `0.3.3`.

### Task 1: Pure random-map and session policy

**Files:**
- Create: `src/victory/MapSessionPolicy.h`
- Create: `src/victory/MapSessionPolicy.cpp`
- Create: `tests/MapSessionPolicyTests.cpp`
- Modify: `src/victory/LaunchOrigin.h`
- Modify: `src/victory/LaunchOrigin.cpp`
- Modify: `tests/LaunchOriginTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `bool IsRandomMapIdentifier(std::wstring_view value) noexcept`
- Produces: `LaunchOriginSnapshot RefineLaunchOrigin(LaunchOriginSnapshot origin, std::wstring_view relativeIdentifier) noexcept`
- Produces: `bool ShouldRecordVictory(LaunchOriginSnapshot origin) noexcept`
- Produces: `bool ShouldShowCompletionMarker(LaunchOriginSnapshot origin) noexcept`
- Produces: `LaunchSource::LoadSinglePlayerMap`

- [ ] **Step 1: Write the failing policy and launch-origin tests**

Add table cases equivalent to:

```cpp
Require(IsRandomMapIdentifier(L"RD_LCGSDR30"), "observed random key");
Require(IsRandomMapIdentifier(L"[seed]"), "square form");
Require(IsRandomMapIdentifier(L"<seed>"), "angle form");
Require(!IsRandomMapIdentifier(L"rd_LCGSDR30"), "case sensitive");
Require(!IsRandomMapIdentifier(L"Map\\User\\RD_Test.map"),
        "relative fixed path is not random");
Require(!IsRandomMapIdentifier(L"[seed"), "unclosed square form");

const LaunchOriginSnapshot loaded{
    LaunchSource::LoadMapUnresolved, SessionEligibility::Unknown};
const auto random = RefineLaunchOrigin(loaded, L"RD_LCGSDR30");
Require(random.source == LaunchSource::RandomMap &&
            random.eligibility == SessionEligibility::Eligible,
        "loaded random map remains recordable");
Require(ShouldRecordVictory(random), "random victory is recorded");
Require(!ShouldShowCompletionMarker(random),
        "random completion marker is hidden");

const auto fixed = RefineLaunchOrigin(
    loaded, L"Map\\User\\Battle of the Gods.map");
Require(fixed.source == LaunchSource::LoadSinglePlayerMap &&
            ShouldRecordVictory(fixed) &&
            ShouldShowCompletionMarker(fixed),
        "confirmed ordinary load is recordable and visible");

const LaunchOriginSnapshot online{
    LaunchSource::LoadOnlineMultiplayer,
    SessionEligibility::ExcludedOnlineMultiplayer};
Require(RefineLaunchOrigin(online, L"RD_LCGSDR30").source ==
            LaunchSource::LoadOnlineMultiplayer,
        "online exclusion retains precedence");
```

Update the fresh-random launch-origin assertion to require
`RandomMap/Eligible` instead of `ExcludedRandomMap`.

- [ ] **Step 2: Run the x86 test target and verify RED**

Run the repository's existing Windows x86 configure/build/test command.
Expected: compilation fails because `MapSessionPolicy` and
`LoadSinglePlayerMap` do not exist, proving the new tests exercise missing
behavior.

- [ ] **Step 3: Implement the minimal pure policy**

Implement the exact game rule:

```cpp
bool IsRandomMapIdentifier(std::wstring_view value) noexcept {
    if (value.rfind(L"RD_", 0u) == 0u) {
        return true;
    }
    return value.size() >= 2u &&
           ((value.front() == L'[' && value.back() == L']') ||
            (value.front() == L'<' && value.back() == L'>'));
}
```

`RefineLaunchOrigin` must return online origins unchanged, convert random
identifiers to `RandomMap/Eligible`, convert only
`LoadMapUnresolved/Unknown` non-random identifiers to
`LoadSinglePlayerMap/Eligible`, and leave other origins unchanged.
`ShouldRecordVictory` returns true exactly for `Eligible`.
`ShouldShowCompletionMarker` returns true only for `Eligible` origins whose
source is neither `RandomMap` nor an online source.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: the new policy tests and all existing launch-origin tests pass.

- [ ] **Step 5: Commit Task 1**

```bash
git add CMakeLists.txt src/victory/LaunchOrigin.h \
  src/victory/LaunchOrigin.cpp src/victory/MapSessionPolicy.h \
  src/victory/MapSessionPolicy.cpp tests/TestMain.cpp \
  tests/LaunchOriginTests.cpp tests/MapSessionPolicyTests.cpp
git commit -m "feat: separate random recording and marker policy"
```

### Task 2: Return confirmed identity observations

**Files:**
- Modify: `src/identity/MapIdentityCoordinator.h`
- Modify: `src/identity/MapIdentityCoordinator.cpp`
- Modify: `tests/MapIdentityCoordinatorTests.cpp`

**Interfaces:**
- Produces: `struct ConfirmedMapIdentity { std::uint64_t sessionId; std::wstring name; std::wstring relative; };`
- Changes: `MapIdentityCoordinator::ObserveTick(...)` returns `std::optional<ConfirmedMapIdentity>`.

- [ ] **Step 1: Write failing coordinator-result tests**

Capture the return from a confirmed observation and assert:

```cpp
const auto identity = confirmed.coordinator.ObserveTick(
    true, 250u, confirmedBridge);
Require(identity.has_value() && identity->sessionId == 1u &&
            identity->name == L"Aeneas" &&
            identity->relative == L"Maps\\Single\\Aeneas.map",
        "confirmed identity is returned for policy refinement");
```

For absolute-path, stale-generation, value-conflict, timeout/name-only, and
disabled cases, assert that the returned optional is empty while the existing
diagnostic trace remains unchanged.

- [ ] **Step 2: Run tests and verify RED**

Expected: compilation fails because `ObserveTick` still returns `void`.

- [ ] **Step 3: Implement the minimal observation return**

Include `<optional>` and `identity/SuMapValue.h` in the coordinator header.
Return `std::nullopt` while no session result is ready. After validation and
diagnostic emission, return `ConfirmedMapIdentity` only when both validated SU
values succeed. Copy `result.sessionId`, `name.normalized`, and
`relative.normalized`; never return raw or invalid bytes.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: all coordinator and existing identity tests pass.

- [ ] **Step 5: Commit Task 2**

```bash
git add src/identity/MapIdentityCoordinator.h \
  src/identity/MapIdentityCoordinator.cpp tests/MapIdentityCoordinatorTests.cpp
git commit -m "feat: expose confirmed map identity observations"
```

### Task 3: Refine the active runtime session and trace it

**Files:**
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/diagnostics/Phase3Trace.cpp`
- Modify: `tests/Phase3TraceTests.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`

**Interfaces:**
- Consumes: `ConfirmedMapIdentity`
- Consumes: `RefineLaunchOrigin(...)`
- Produces trace: `origin-refinement=session-<id>;source-<name>;eligibility-<name>;ui-<visible|hidden>`

- [ ] **Step 1: Write failing trace and runtime-wiring tests**

Require the origin channel to admit:

```cpp
Require(first.Write(
            Phase3TraceChannel::Origin,
            "origin-refinement=session-3;source-random-map;eligibility-eligible;ui-hidden"),
        "origin allowlist accepts bounded identity refinement");
```

Keep absolute paths, line breaks, and unrelated prefixes rejected. Extend the
source-inspection policy test to require `RefineLaunchOrigin`,
`identity->sessionId == activeSessionId_`, and `origin-refinement=` in
`S4Listeners.cpp`.

- [ ] **Step 2: Run tests and verify RED**

Expected: the trace write and runtime source-policy assertion fail because the
prefix and wiring do not exist.

- [ ] **Step 3: Implement session-bound runtime refinement**

In `ObserveTick`, capture the coordinator result. Refine only when its session
ID equals `activeSessionId_`. Set `activeOrigin_` through
`RefineLaunchOrigin(activeOrigin_, identity->relative)`, then emit the bounded
origin record using `LaunchSourceName`, `SessionEligibilityName`, and
`ShouldShowCompletionMarker`. Add only `origin-refinement=` to the origin
trace allowlist; do not relax forbidden tokens or path restrictions.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: all trace, policy, coordinator, and runtime-policy tests pass.

- [ ] **Step 5: Commit Task 3**

```bash
git add src/runtime/S4Listeners.cpp src/diagnostics/Phase3Trace.cpp \
  tests/Phase3TraceTests.cpp tests/RuntimePolicyTests.cpp
git commit -m "feat: refine map origin from confirmed SU identity"
```

### Task 4: Version and full verification

**Files:**
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `docs/research/phase-3a-victory-ui-calibration-report.md`

**Interfaces:**
- Produces: diagnostic build version `0.3.3`.

- [ ] **Step 1: Write the failing version-policy assertion**

Change the runtime policy test to require `Version=0.3.3` and
`version=0.3.3`, while continuing to require `CompletionDetection=0`,
`CompletionStorage=0`, and `CompletionMarkers=0`.

- [ ] **Step 2: Run tests and verify RED**

Expected: version assertions fail against the current `0.3.2` configuration
and runtime header.

- [ ] **Step 3: Implement the version and evidence update**

Set the INI and runtime diagnostic header to `0.3.3`. Append a dated report
section documenting the paired fresh/load random identity, the game-owned
identifier rule, and the approved recordable-but-marker-hidden policy. Do not
claim persistence or rendering is enabled.

- [ ] **Step 4: Run complete verification**

Run, in order:

```text
git diff --check
Windows x86 CMake configure
Windows x86 build with /W4 /WX
CTest
repository policy/deployment preflight in read-only mode
```

Expected: every command exits zero, all tests pass, and no game-directory file
is written.

- [ ] **Step 5: Commit Task 4**

```bash
git add config/CampaignCompletionDebug.ini src/runtime/DiagnosticRuntime.cpp \
  tests/RuntimePolicyTests.cpp \
  docs/research/phase-3a-victory-ui-calibration-report.md
git commit -m "docs: record random load source resolution"
```

- [ ] **Step 6: Request code review before deployment**

Use the requesting-code-review and verification-before-completion workflows.
Report the exact test/build evidence, current game-running deployment blocker,
and resulting ASI hash. Do not deploy until the user closes the game and gives
explicit deployment approval.
