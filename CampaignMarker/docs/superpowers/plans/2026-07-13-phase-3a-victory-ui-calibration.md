# Phase 3A Victory UI Calibration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a public-S4ModApi-only Win32 calibration ASI that records bounded launch/load origin, SU map identity, and after-game GUI features without making a victory or completion decision.

**Architecture:** Keep launch origin, map identity, settlement feature capture, and trace output in focused components. `S4Listeners` routes public page, map-init, Lua-open, tick, and GUI-element callbacks to those components; no internal Hook, Lua write, save write, completion storage, or marker code is linked. This Phase 3A plan intentionally stops after collecting victory/defeat/exit and load-source evidence, because exact UI signatures and random-save metadata cannot be safely hard-coded before live samples exist.

**Tech Stack:** C++17, Win32 x86, S4ModApi 2.3.2 public observers, Settlers United read-only Lua 3.2.1 bridge, CMake 3.24+, MSVC `/W4 /WX /permissive-`, Google-free custom test harness, GitHub Actions `windows-2022`.

## Global Constraints

- Work directly on `main`; do not create a branch or worktree.
- Never modify or delete a game-directory file except the user-authorized project-owned `CampaignCompletion*.asi`, its configuration directory, and an exact guarded replacement of `Plugin_SU.zip`.
- Do not replace `Plugin_SU.zip` unless the game is closed, the immutable original backup is verified, and the user approves the deployment step.
- Never start, close, or kill the game or Settlers United on the user's behalf; the user performs all game navigation and gameplay.
- Write calibration output only below the configured project trace root; the packaged `CaptureTraceRoot` remains empty.
- Do not call `GameDefaultGameEndCheck`, replace Lua functions, write Lua state, patch process code, install an internal Hook, write save data, persist completion, or render markers.
- Offline single-player `Multiplayer Maps` are eligible evidence. Only online multiplayer and random-map sources are excluded.
- Load-game source recovery is required evidence; ordinary loaded maps may remain `load-map-unresolved` only during Phase 3A calibration and block Phase 3 GO until resolved.
- One map-init session opens at most one bounded settlement capture.
- The authoritative verification is a Win32 GitHub Actions build; require two clean runs before deployment.

---

## File Structure

Create these focused units:

- `src/victory/LaunchOrigin.h/.cpp`: public-page and fixed-tab launch/load source state machine.
- `src/victory/SettlementUiProbe.h/.cpp`: numeric GUI feature normalization, deduplication, and bounded capture.
- `src/diagnostics/Phase3Trace.h/.cpp`: four-channel project-scoped trace directory with explicit record allowlists.
- `tests/LaunchOriginTests.cpp`: origin precedence, offline/online distinction, random and load cases.
- `tests/SettlementUiProbeTests.cpp`: capture lifetime, bounds, deduplication, and one-shot behavior.
- `tests/Phase3TraceTests.cpp`: directory admission, unique session directory, channel allowlists, and forbidden data.

Modify these existing units:

- `src/identity/MapIdentityCoordinator.h/.cpp`: confirm valid SU identity without requiring a fixed-list epoch and expose the current map-init session ID.
- `src/runtime/S4Listeners.h/.cpp`: route public callbacks to origin and settlement capture.
- `src/runtime/DiagnosticRuntime.h/.cpp`: own Phase 3 components, configure project trace output, and preserve stop ordering.
- `tests/MapIdentityCoordinatorTests.cpp`: cover non-list campaign/load identity.
- `tests/RuntimePolicyTests.cpp`: enforce Phase 3A zero-Hook, zero-write, public-observer policy.
- `tests/TestMain.cpp`, `CMakeLists.txt`: register the new production and test sources.
- `config/CampaignCompletionDebug.ini`: version `0.3.0`, calibration mode, and explicit disabled behaviors.
- `tools/verify_no_process_patch.ps1`: reject victory-handler calls and Lua writes in the new source directory.
- `docs/research/phase-3a-victory-ui-calibration-report.md`: record CI, artifact, deployment, samples, and the evidence gate after live work.

---

### Task 1: Public launch and load origin state machine

**Files:**
- Create: `src/victory/LaunchOrigin.h`
- Create: `src/victory/LaunchOrigin.cpp`
- Create: `tests/LaunchOriginTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: raw `S4_GUI_ENUM` page observations, `FixedMapListKind`, and monotonic milliseconds.
- Produces: `LaunchOriginTracker::ConsumeMapInit(std::uint64_t) -> LaunchOriginSnapshot`.

- [ ] **Step 1: Write the failing origin tests**

```cpp
// tests/LaunchOriginTests.cpp
#include "victory/LaunchOrigin.h"

#include <stdexcept>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int RunLaunchOriginTests() {
    using namespace campaign_completion;

    LaunchOriginTracker offlineMulti;
    offlineMulti.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI, 100u);
    offlineMulti.ObserveListKind(FixedMapListKind::Multiplayer, 110u);
    const auto eligible = offlineMulti.ConsumeMapInit(200u);
    Require(eligible.source == LaunchSource::SinglePlayerMultiplayerMap,
            "offline Multiplayer Maps source retained");
    Require(eligible.eligibility == SessionEligibility::Eligible,
            "offline Multiplayer Maps remain eligible");

    LaunchOriginTracker online;
    online.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI, 100u);
    online.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 150u);
    Require(online.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online lobby is excluded");

    LaunchOriginTracker random;
    random.ObservePage(S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM, 100u);
    Require(random.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedRandomMap,
            "single-player random map is excluded");

    LaunchOriginTracker onlineRandom;
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM, 100u);
    onlineRandom.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 110u);
    Require(onlineRandom.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "online session origin takes precedence over random map type");

    LaunchOriginTracker loadedMap;
    loadedMap.ObservePage(S4_SCREEN_LOADGAME_MAP, 100u);
    const auto unresolved = loadedMap.ConsumeMapInit(200u);
    Require(unresolved.source == LaunchSource::LoadMapUnresolved &&
                unresolved.eligibility == SessionEligibility::Unknown,
            "ordinary loaded map waits for random-source evidence");

    LaunchOriginTracker loadedCampaign;
    loadedCampaign.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 100u);
    Require(loadedCampaign.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::Eligible,
            "campaign load is eligible");

    LaunchOriginTracker loadedOnline;
    loadedOnline.ObservePage(S4_SCREEN_LOADGAME_MULTIPLAYER, 100u);
    Require(loadedOnline.ConsumeMapInit(200u).eligibility ==
                SessionEligibility::ExcludedOnlineMultiplayer,
            "multiplayer load is excluded");

    LaunchOriginTracker expired;
    expired.ObservePage(S4_SCREEN_LOADGAME_CAMPAIGN, 1u);
    Require(expired.ConsumeMapInit(1u + kLaunchOriginLeaseMs).source ==
                LaunchSource::Unknown,
            "expired origin fails closed");
    Require(expired.ConsumeMapInit(2u + kLaunchOriginLeaseMs).source ==
                LaunchSource::Unknown,
            "origin is consumed once");
    return 0;
}
```

Add `int RunLaunchOriginTests();` and call it before runtime-policy tests in
`tests/TestMain.cpp`. Add the new test and source to the test target in
`CMakeLists.txt`.

- [ ] **Step 2: Run CI to verify the tests fail**

Run:

```bash
git add tests/LaunchOriginTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: define launch origin classification"
git push origin main
gh workflow run build-debug-asi.yml --ref main
gh run watch --exit-status
```

Expected: Win32 build fails because `victory/LaunchOrigin.h` does not exist.

- [ ] **Step 3: Implement the minimal state machine**

```cpp
// src/victory/LaunchOrigin.h
#pragma once

#include "S4ModApi.h"
#include "identity/ListAttribution.h"

#include <cstdint>
#include <string_view>

namespace campaign_completion {

constexpr std::uint64_t kLaunchOriginLeaseMs = 30'000u;

enum class LaunchSource {
    Unknown,
    SinglePlayerMap,
    SinglePlayerMultiplayerMap,
    SinglePlayerCustomMap,
    Campaign,
    RandomMap,
    OnlineMultiplayer,
    LoadCampaign,
    LoadMapUnresolved,
    LoadOnlineMultiplayer,
};

enum class SessionEligibility {
    Unknown,
    Eligible,
    ExcludedOnlineMultiplayer,
    ExcludedRandomMap,
};

struct LaunchOriginSnapshot final {
    LaunchSource source = LaunchSource::Unknown;
    SessionEligibility eligibility = SessionEligibility::Unknown;
};

std::string_view LaunchSourceName(LaunchSource value) noexcept;
std::string_view SessionEligibilityName(SessionEligibility value) noexcept;

class LaunchOriginTracker final {
public:
    void ObservePage(DWORD page, std::uint64_t nowMs) noexcept;
    void ObserveListKind(FixedMapListKind kind,
                         std::uint64_t nowMs) noexcept;
    void ObserveBack() noexcept;
    LaunchOriginSnapshot ConsumeMapInit(std::uint64_t nowMs) noexcept;
    void Disable() noexcept;

private:
    void Set(LaunchOriginSnapshot value, std::uint64_t nowMs) noexcept;

    LaunchOriginSnapshot current_{};
    std::uint64_t observedAtMs_ = 0u;
    bool enabled_ = true;
};

}  // namespace campaign_completion
```

Implement `ObservePage` with explicit `switch` cases. Online multiplayer pages
and `S4_SCREEN_LOADGAME_MULTIPLAYER` set the online exclusion and cannot be
downgraded by a later multiplayer random-map page; random selectors set random
exclusion only while the epoch is not already online; campaign/menu pages 2
through 21 and campaign load set eligible; ordinary load sets
`LoadMapUnresolved/Unknown`. `ObserveListKind`
maps Single, Multiplayer, and Custom to three eligible sources. `ConsumeMapInit`
returns `Unknown` at or beyond the lease boundary, clears the stored value in
all cases, and never infers from a map path.

- [ ] **Step 4: Run the focused and full tests**

Run the Win32 workflow as in Step 2.

Expected: `campaign_completion_tests` passes and the package still contains
only the INI and ASI.

- [ ] **Step 5: Commit**

```bash
git add src/victory/LaunchOrigin.h src/victory/LaunchOrigin.cpp \
  tests/LaunchOriginTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: classify public launch and load origins"
```

---

### Task 2: Generalize SU map identity beyond fixed-list launches

**Files:**
- Modify: `src/identity/MapIdentityCoordinator.h`
- Modify: `src/identity/MapIdentityCoordinator.cpp`
- Modify: `tests/MapIdentityCoordinatorTests.cpp`

**Interfaces:**
- Consumes: the existing `LuaMapSessionResult` and optional fixed-list evidence.
- Produces: `CurrentSessionId() -> std::uint64_t`; valid name and relative path emit `identity-association=confirmed` even when the list is unknown.

- [ ] **Step 1: Change the test to require list-independent identity**

Replace the existing no-list assertion with:

```cpp
Require(Contains(noList.trace, "map-init-session=1;list=unknown") &&
            Contains(noList.trace, "identity-association=confirmed"),
        "campaign and load identity confirm without a fixed-list epoch");
Require(noList.coordinator.CurrentSessionId() == 1u,
        "coordinator exposes the active map-init session");
```

Add a test proving `ObserveBack()` clears the active session ID and pending
identity, then a later `ObserveMapInit()` returns session ID 2 rather than
reusing session ID 1.

- [ ] **Step 2: Run CI and verify the changed test fails**

Expected: failure because no-list identity currently emits
`identity-association=no-list-epoch` and `CurrentSessionId` is absent.

- [ ] **Step 3: Implement the minimal change**

Add to the public interface:

```cpp
std::uint64_t CurrentSessionId() const noexcept { return currentSessionId_; }
```

Store the ID returned by `session_.ObserveMapInit(nowMs)` in
`currentSessionId_`. In `EmitResult`, remove the `sessionList_ == Unknown`
rejection and select `confirmed` whenever both validated values are present.
Keep the list name in the map-init trace as independent origin evidence. Reset
the active `currentSessionId_` to zero in `ObserveBack` and `Disable`; the
monotonic counter remains owned by `LuaMapSession` and is not reset by Back.

- [ ] **Step 4: Run the full Win32 test suite**

Expected: all identity tests pass; stale-generation, conflict, invalid-path,
and timeout behavior remains unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/identity/MapIdentityCoordinator.h \
  src/identity/MapIdentityCoordinator.cpp \
  tests/MapIdentityCoordinatorTests.cpp
git commit -m "feat: confirm SU identity for campaign and load sessions"
```

---

### Task 3: Bounded after-game GUI feature capture

**Files:**
- Create: `src/victory/SettlementUiProbe.h`
- Create: `src/victory/SettlementUiProbe.cpp`
- Create: `tests/SettlementUiProbeTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: numeric fields copied from `S4GuiElementBltParams` during the callback.
- Produces: `SettlementCapture` containing at most 256 unique features after a 2,500 ms window.

- [ ] **Step 1: Write failing bounded-capture tests**

```cpp
// tests/SettlementUiProbeTests.cpp
#include "victory/SettlementUiProbe.h"

#include <stdexcept>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int RunSettlementUiProbeTests() {
    using namespace campaign_completion;
    SettlementUiProbe probe;
    SettlementFeature first{};
    first.gfxCollection = 4u;
    first.mainTexture = 120u;
    first.valueLink = 55u;
    first.x = 10u;
    first.y = 20u;

    Require(probe.Begin(7u, 100u), "first capture begins");
    Require(!probe.Begin(7u, 101u), "capture cannot begin twice");
    probe.Observe(first);
    probe.Observe(first);
    Require(!probe.FinishIfDue(100u + kSettlementCaptureMs - 1u).has_value(),
            "capture remains open before deadline");
    const auto capture =
        probe.FinishIfDue(100u + kSettlementCaptureMs);
    Require(capture.has_value() && capture->sessionId == 7u &&
                capture->features.size() == 1u,
            "deadline returns one deduplicated feature");
    Require(!probe.FinishIfDue(100u + kSettlementCaptureMs + 1u).has_value(),
            "finished capture is one shot");

    SettlementUiProbe bounded;
    Require(bounded.Begin(8u, 0u), "bounded capture begins");
    for (std::uint16_t index = 0u;
         index < static_cast<std::uint16_t>(kMaxSettlementFeatures + 5u);
         ++index) {
        SettlementFeature feature{};
        feature.mainTexture = index;
        bounded.Observe(feature);
    }
    const auto capped = bounded.FinishIfDue(kSettlementCaptureMs);
    Require(capped.has_value() &&
                capped->features.size() == kMaxSettlementFeatures &&
                capped->truncated,
            "capture is count bounded and reports truncation");
    bounded.Disable();
    Require(!bounded.Begin(9u, 0u), "disabled probe remains inert");
    return 0;
}
```

- [ ] **Step 2: Run CI and verify missing-type failure**

Expected: build fails because `SettlementUiProbe.h` is absent.

- [ ] **Step 3: Implement the pure-data probe**

```cpp
// src/victory/SettlementUiProbe.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace campaign_completion {

constexpr std::uint64_t kSettlementCaptureMs = 2'500u;
constexpr std::size_t kMaxSettlementFeatures = 256u;

struct SettlementFeature final {
    std::uint16_t gfxCollection = 0u;
    std::uint16_t containerType = 0u;
    std::uint16_t x = 0u;
    std::uint16_t y = 0u;
    std::uint16_t width = 0u;
    std::uint16_t height = 0u;
    std::uint16_t mainTexture = 0u;
    std::uint16_t valueLink = 0u;
    std::uint16_t buttonPressedTexture = 0u;
    std::uint16_t tooltipLink = 0u;
    std::uint16_t imageStyle = 0u;
    std::uint16_t effects = 0u;
    std::uint16_t textStyle = 0u;
    std::uint16_t showTexture = 0u;
    std::uint16_t backTexture = 0u;

    bool operator==(const SettlementFeature& other) const noexcept;
};

struct SettlementCapture final {
    std::uint64_t sessionId = 0u;
    std::vector<SettlementFeature> features;
    bool truncated = false;
};

class SettlementUiProbe final {
public:
    bool Begin(std::uint64_t sessionId, std::uint64_t nowMs) noexcept;
    void Observe(const SettlementFeature& feature) noexcept;
    std::optional<SettlementCapture> FinishIfDue(
        std::uint64_t nowMs) noexcept;
    void Disable() noexcept;
    bool Active() const noexcept { return active_; }

private:
    SettlementCapture capture_{};
    std::uint64_t startedAtMs_ = 0u;
    bool active_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
```

Implement equality over all fields. `Observe` linearly deduplicates a maximum
of 256 entries; the small fixed bound avoids a hash allocator and preserves
deterministic output order. Catch allocation exceptions inside every `noexcept`
method, set `truncated`, and never allow an exception through a game callback.

- [ ] **Step 4: Run focused and full Win32 tests**

Expected: deduplication, exact deadline, cap, one-shot finish, and Disable pass.

- [ ] **Step 5: Commit**

```bash
git add src/victory/SettlementUiProbe.h \
  src/victory/SettlementUiProbe.cpp tests/SettlementUiProbeTests.cpp \
  tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: capture bounded settlement UI features"
```

---

### Task 4: Four-channel project-scoped Phase 3 trace

**Files:**
- Create: `src/diagnostics/Phase3Trace.h`
- Create: `src/diagnostics/Phase3Trace.cpp`
- Create: `tests/Phase3TraceTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Phase3TraceChannel` plus one newline-free allowlisted record.
- Produces: a unique `phase-3-session-<pid>[-suffix]` directory containing `origin.trace`, `identity.trace`, `settlement-ui.trace`, and `decision.trace`.

- [ ] **Step 1: Write failing trace policy tests**

```cpp
// Core assertions to add in tests/Phase3TraceTests.cpp
Phase3Trace trace;
Require(trace.Open(root, 42u), "admitted project root opens");
Require(trace.Write(Phase3TraceChannel::Origin,
                    "origin-source=single-player-multiplayer-map"),
        "origin allowlist accepts source");
Require(trace.Write(Phase3TraceChannel::Identity,
                    "identity-association=confirmed"),
        "identity allowlist accepts association");
Require(trace.Write(Phase3TraceChannel::SettlementUi,
                    "settlement-capture=session-1;features=3;truncated=0"),
        "settlement allowlist accepts summary");
Require(!trace.Write(Phase3TraceChannel::SettlementUi,
                     "chat=private text"),
        "unrelated text is rejected");
Require(!trace.Write(Phase3TraceChannel::Origin,
                     "save=C:\\secret\\game.sav"),
        "absolute save path is rejected");
Require(!trace.Write(Phase3TraceChannel::Decision,
                     "victory-confirmed"),
        "Phase 3A cannot claim a victory decision");
trace.Close();
Require(std::filesystem::exists(trace.directory() / "origin.trace") &&
            std::filesystem::exists(trace.directory() / "identity.trace") &&
            std::filesystem::exists(trace.directory() /
                                    "settlement-ui.trace") &&
            std::filesystem::exists(trace.directory() / "decision.trace"),
        "all four bounded channels exist");
```

Reuse the existing temporary-directory and reparse/admission patterns from
`CaptureTraceTests.cpp`. Add a second instance with the same PID and require a
different session directory.

- [ ] **Step 2: Run CI and verify missing-class failure**

Expected: build fails because `Phase3Trace` is undefined.

- [ ] **Step 3: Implement trace admission and channel allowlists**

```cpp
// src/diagnostics/Phase3Trace.h
#pragma once

#include <windows.h>

#include <array>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace campaign_completion {

enum class Phase3TraceChannel : std::size_t {
    Origin = 0u,
    Identity = 1u,
    SettlementUi = 2u,
    Decision = 3u,
};

class Phase3Trace final {
public:
    bool Open(const std::filesystem::path& root, DWORD processId);
    bool Write(Phase3TraceChannel channel, std::string_view record);
    void Close() noexcept;
    const std::filesystem::path& directory() const noexcept {
        return directory_;
    }

private:
    std::mutex mutex_;
    std::array<HANDLE, 4u> handles_{INVALID_HANDLE_VALUE,
                                   INVALID_HANDLE_VALUE,
                                   INVALID_HANDLE_VALUE,
                                   INVALID_HANDLE_VALUE};
    std::filesystem::path directory_;
};

}  // namespace campaign_completion
```

Use `GetFileAttributesW` plus a final-path handle to reject missing roots and
reparse points. Create a unique child directory with suffixes 0 through 99,
then create the four exact files with `CREATE_NEW`. If any file fails, close
all opened handles and remove only the empty project-created session directory.
Allow prefixes per channel:

```text
origin: origin-source=, origin-eligibility=, origin-status=
identity: lua-open-generation=, map-init-session=, su-map-name-status=,
          su-map-relative-status=, su-map-name=, su-map-relative=,
          identity-association=
settlement-ui: settlement-capture=, settlement-feature=,
               local-player-status=, local-player-lost=
decision: diagnostic-result=calibration-only, controlled-stop-flush=
```

Reject CR/LF, `0x`, absolute paths, `chat=`, `player-name=`,
`completed_maps`, `completion`, `persistence`, `marker`, and
`victory-confirmed`. Cap one record at 1,024 bytes and flush each successful
write.

- [ ] **Step 4: Run full Win32 tests**

Expected: all four files and unique directories are created only below an
admitted root; forbidden records are rejected.

- [ ] **Step 5: Commit**

```bash
git add src/diagnostics/Phase3Trace.h src/diagnostics/Phase3Trace.cpp \
  tests/Phase3TraceTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: add bounded phase 3 diagnostic traces"
```

---

### Task 5: Integrate public callbacks into the calibration runtime

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `src/identity/MapIdentityCoordinator.h`
- Modify: `src/identity/MapIdentityCoordinator.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1–4 components and existing public listeners.
- Produces: one Phase 3A session directory per process and a calibration-only settlement snapshot for each map session reaching after-game summary.

- [ ] **Step 1: Write failing runtime policy assertions**

Update `RuntimePolicyTests.cpp` to require:

```cpp
for (const auto* required : {
         "Version=0.3.0", "DiagnosticMode=VictoryUiCalibration",
         "PublicSettlementUiProbe=1", "LaunchOriginTracking=1",
         "LoadOriginRecovery=1", "InternalVictoryHook=0",
         "GameDefaultGameEndCheckCalls=0", "LuaWrites=0",
         "GameDataWrites=0", "CompletionStorage=0",
         "CompletionMarkers=0", "CaptureTraceRoot="}) {
    Require(policy.find(required) != std::string::npos,
            "phase 3A policy field missing");
}
Require((runtime + runtimeHeader + listeners).find(
            "GameDefaultGameEndCheck") == std::string::npos,
        "runtime must never call the behavior-producing end check");
Require(runtimeHeader.find("Phase3Trace") != std::string::npos &&
            runtimeHeader.find("LaunchOriginTracker") != std::string::npos &&
            runtimeHeader.find("SettlementUiProbe") != std::string::npos,
        "runtime owns the public calibration pipeline");
Require(listeners.find("AddGuiElementBltListener(&OnGuiElement)") !=
            std::string::npos,
        "public GUI-element observer remains the settlement source");
```

Also require `SettlementUiProbe::Disable`, `LaunchOriginTracker::Disable`, and
`MapIdentityCoordinator::Disable` to occur before `listeners_.Stop()`, and
trace close to occur afterward.

- [ ] **Step 2: Run CI and verify policy failure**

Expected: failure because version 0.3.0 and Phase 3 runtime ownership are not
implemented.

- [ ] **Step 3: Extend listener construction and routing**

Change `S4Listeners::Start` to accept references to `LaunchOriginTracker`,
`SettlementUiProbe`, and `Phase3Trace`. Add these fields:

```cpp
LaunchOriginTracker* origin_ = nullptr;
SettlementUiProbe* settlement_ = nullptr;
Phase3Trace* phase3Trace_ = nullptr;
LaunchOriginSnapshot activeOrigin_{};
std::uint64_t activeSessionId_ = 0u;
bool inGameSeen_ = false;
bool settlementStarted_ = false;
```

Route events as follows:

```cpp
// At the start of ObserveUiFrame, under the existing listener mutex:
origin_->ObservePage(page, now);
if (page == S4_SCREEN_INGAME) inGameSeen_ = true;
if (page == S4_SCREEN_AFTERGAME_SUMMARY && inGameSeen_ &&
    !settlementStarted_ && activeSessionId_ != 0u) {
    settlementStarted_ = settlement_->Begin(activeSessionId_, now);
}

// In ObserveMapInit:
activeOrigin_ = origin_->ConsumeMapInit(now);
activeSessionId_ = coordinator_->ObserveMapInit(now);
inGameSeen_ = false;
settlementStarted_ = false;

// In the fixed-tab click block:
origin_->ObserveListKind(listKind, now);

// In ObserveGuiElement, only during an active settlement capture:
SettlementFeature feature{};
feature.gfxCollection = element->currentGFXCollection;
feature.containerType = element->containerType;
feature.x = element->x;
feature.y = element->y;
feature.width = element->width;
feature.height = element->height;
feature.mainTexture = element->mainTexture;
feature.valueLink = element->valueLink;
feature.buttonPressedTexture = element->buttonPressedTexture;
feature.tooltipLink = element->tooltipLink;
feature.imageStyle = static_cast<std::uint16_t>(element->imageStyle);
feature.effects = static_cast<std::uint16_t>(element->effects);
feature.textStyle = static_cast<std::uint16_t>(element->textStyle);
feature.showTexture = element->showTexture;
feature.backTexture = element->backTexture;
settlement_->Observe(feature);
```

Do not dereference `text`, `tooltipText`, or `tooltipExtraText` in Phase 3A.

During non-delayed Tick, finish a due settlement capture. At that moment call
`GetLocalPlayer()` once; call `HasPlayerLost(localPlayer)` only when the player
ID is nonzero. Emit origin, identity, local-player state, capture summary, and
one `settlement-feature=` line per bounded feature. The decision channel emits
only `diagnostic-result=calibration-only`.

- [ ] **Step 4: Upgrade runtime ownership, config, and stop ordering**

In `DiagnosticRuntime`, replace `CaptureTrace` ownership with:

```cpp
Phase3Trace phase3Trace_;
LaunchOriginTracker origin_;
SettlementUiProbe settlement_;
```

Read the existing `CaptureTraceRoot` key. Open `Phase3Trace` only when the key
is nonempty and admitted. Send `MapIdentityCoordinator` trace records to the
Identity channel. Update the bootstrap header to:

```text
CampaignCompletionDebug bootstrap version=0.3.0 mode=victory-ui-calibration-public
```

On Stop: disable coordinator, origin, and settlement; stop listeners; release
the API; write `controlled-stop-flush=success|failed`; close trace; close log.
Clear every new listener pointer and session field in `S4Listeners::Stop`.

Set the packaged INI to:

```ini
; CampaignCompletionDebug phase-3A public victory UI calibration settings.
[Diagnostic]
Version=0.3.0
LogLevel=Info
ModuleInventory=1
PublicListeners=1
CaptureTraceRoot=
DiagnosticMode=VictoryUiCalibration
IdentitySource=SettlersUnitedLua
PublicSettlementUiProbe=1
LaunchOriginTracking=1
LoadOriginRecovery=1
InternalVictoryHook=0
HookCount=0
CodePatchBytes=0
GameDefaultGameEndCheckCalls=0
LuaWrites=0
GameDataWrites=0
CompletionDetection=0
CompletionStorage=0
CompletionMarkers=0
ControlledStopFile=CampaignCompletionDebug.stop
```

- [ ] **Step 5: Run two full clean Win32 CI runs**

Run:

```bash
git add src/runtime src/identity config/CampaignCompletionDebug.ini \
  tests/RuntimePolicyTests.cpp CMakeLists.txt
git commit -m "feat: integrate public victory UI calibration"
git push origin main
gh workflow run build-debug-asi.yml --ref main
gh run watch --exit-status
gh workflow run build-debug-asi.yml --ref main
gh run watch --exit-status
```

Expected for both runs: configure, build, policy verification, tests, package,
PE32 check, and artifact upload all succeed.

---

### Task 6: Harden policy verification and freeze the calibration artifact

**Files:**
- Modify: `tools/verify_no_process_patch.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`
- Create: `docs/research/phase-3a-victory-ui-calibration-report.md`

**Interfaces:**
- Consumes: the linked ASI, all production sources, two successful CI runs, and the downloaded artifact.
- Produces: a frozen artifact hash and an evidence report that makes no victory/completion claim.

- [ ] **Step 1: Add failing forbidden fixtures to the workflow**

Add fixtures that append each forbidden token to copied production sources and
require verification rejection:

```powershell
$endCheckFixture = New-PolicyFixture "campaign-end-check-policy"
$listener = Join-Path $endCheckFixture "src/runtime/S4Listeners.cpp"
[IO.File]::AppendAllText($listener,
  "`n// GameDefaultGameEndCheck forbidden behavior fixture`n")
Assert-Rejected $endCheckFixture `
  "GameDefaultGameEndCheck fixture was unexpectedly accepted"

$luaHandlerFixture = New-PolicyFixture "campaign-lua-handler-policy"
$listener = Join-Path $luaHandlerFixture "src/runtime/S4Listeners.cpp"
[IO.File]::AppendAllText($listener,
  "`n// VICTORY_CONDITION_CHECK mutation fixture`n")
Assert-Rejected $luaHandlerFixture `
  "Lua victory-handler fixture was unexpectedly accepted"
```

- [ ] **Step 2: Run CI and verify the unextended verifier accepts a fixture**

Expected: the workflow fails with one of the “unexpectedly accepted” messages.

- [ ] **Step 3: Extend the verifier**

Reject, case-insensitively, production-source occurrences of:

```text
GameDefaultGameEndCheck
DefaultGameEndCheck
VICTORY_CONDITION_CHECK
CStateVictoryScreen
lua_setglobal
lua_settable
hlib::CallPatch
WriteProcessMemory
VirtualProtect
```

Allow those strings only in the verifier script, tests, docs, and the deliberate
workflow fixtures. Preserve existing import-table and PE checks.

- [ ] **Step 4: Run two clean CI workflows and freeze one artifact**

Expected: both runs succeed. Download the later artifact to a project-scoped
`artifacts/phase-3a-ci/<run-id>/` directory, verify the GitHub artifact digest,
unpack it, and record SHA-256 for the ZIP, ASI, and INI in the report.

- [ ] **Step 5: Write the pre-live report and commit**

The report must contain exact commit, workflow/run/job IDs, artifact ID and
digest, ZIP/ASI/INI hashes, PE machine `0x014c`, package file list, policy
results, and this statement:

```text
This calibration build does not decide victory, persist completion, or render
markers. It records bounded public GUI and source evidence only.
```

Commit:

```bash
git add tools/verify_no_process_patch.ps1 \
  .github/workflows/build-debug-asi.yml \
  docs/research/phase-3a-victory-ui-calibration-report.md
git commit -m "test: harden phase 3A public calibration policy"
```

---

### Task 7: Guarded deployment and live evidence gate

**Files:**
- Modify only after live work: `docs/research/phase-3a-victory-ui-calibration-report.md`
- Create only under project root: `artifacts/phase-3-victory-diagnostics/<session-id>/...`
- Authorized guarded replacement: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`

**Interfaces:**
- Consumes: frozen ASI, verified immutable original archive backup, user-closed game, explicit deployment approval, and user-driven test actions.
- Produces: exit/victory/defeat GUI samples plus new/load origin samples; no decision signature or completion record.

- [ ] **Step 1: Stop before deployment and obtain readiness**

Confirm the game is closed. Re-read the archive and immutable-backup hashes.
Require the immutable original hash
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.
Ask for explicit approval before replacing the archive.

- [ ] **Step 2: Perform the existing guarded exact-entry replacement**

Use the existing project deployment script with the frozen artifact. Replace
only `Plugins/CampaignCompletionDebug.asi`, preserve every other ZIP entry,
and set the live INI trace root to the Windows path corresponding to the
project `artifacts/phase-3-victory-diagnostics` directory. Record original,
patched, and embedded ASI hashes.

- [ ] **Step 3: Capture three settlement controls**

Ask the user, one action at a time, to produce:

1. an eligible map voluntary exit;
2. an eligible map normal victory; and
3. an eligible map normal defeat.

After each action, copy only the newly created project trace session into an
immutable evidence subdirectory and verify that it contains one bounded
settlement capture and `diagnostic-result=calibration-only`.

- [ ] **Step 4: Capture launch/load source controls**

Ask the user, one action at a time, to exercise fresh and loaded cases for:

1. Single Player Maps;
2. offline single-player Multiplayer Maps;
3. Custom Maps;
4. campaign;
5. random map; and
6. online multiplayer save.

For ordinary loaded maps, record `load-map-unresolved` plus the SU name and
relative path; do not guess random status. Copy only user-created or explicitly
designated save samples if SU evidence cannot distinguish fixed/custom from
random.

- [ ] **Step 5: Controlled stop and report the Phase 3A gate**

Request controlled stop, verify every public listener is removed with zero
failures, trace flush succeeds, and ask the user to confirm the game remains
responsive. Update the report with exact sample hashes and one conclusion:

- `GO-TO-PHASE-3B` when public GUI features are stable/disjoint and load-source
  evidence is sufficient to write exact classifier rules; or
- `EVIDENCE-GAP` with the exact missing public GUI or load-source fact.

Do not write a victory classifier, save parser, or internal Hook in this task.
Those require an evidence-derived design/plan with concrete signatures or
validated metadata fields.

- [ ] **Step 6: Commit the final Phase 3A report**

```bash
git add docs/research/phase-3a-victory-ui-calibration-report.md
git commit -m "docs: report phase 3A victory UI calibration"
```

## Plan self-review checklist

- Every approved public-only Phase 3A constraint maps to a task.
- Offline `Multiplayer Maps` and online multiplayer are tested as distinct.
- Random fresh maps are excluded; ordinary loaded maps remain explicitly
  unresolved until evidence proves their source.
- Campaign and load identity no longer require a fixed-list epoch.
- Victory, defeat, and voluntary exit are captured but not classified.
- No task hard-codes an unobserved GUI signature or save offset.
- Trace files are unique, project-scoped, bounded, and allowlisted.
- Internal Hook, Lua mutation, game-end invocation, persistence, and markers
  remain absent and are enforced by negative fixtures.
- Deployment remains separately gated by game closure and explicit approval.
