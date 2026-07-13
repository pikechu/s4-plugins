# Phase 3A Live Calibration Corrections Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct offline launch-origin classification, finish the bounded settlement probe from UI-frame callbacks, and redeploy the calibration INI to the directory from which the loaded ASI resolves configuration.

**Architecture:** `LaunchOriginTracker` gains a navigation-context state machine so simultaneously drawn sibling pages cannot claim an online or specific fixed-list origin. `S4Listeners` centralizes one-shot settlement finalization and invokes it from both non-delayed Tick and UI-frame callbacks. Deployment continues to use the guarded exact ZIP-entry installer, while the live INI is derived from the frozen INI and written only to the authorized game `CampaignCompletion` directory.

**Tech Stack:** C++17, Win32/S4ModApi public listeners, CMake/CTest with MSVC Win32 `/W4 /WX /permissive-`, PowerShell guarded packaging, GitHub Actions.

## Global Constraints

- Work directly on `main`; do not create a worktree or feature branch.
- Never terminate the game or Settlers United; the user starts, navigates, and closes them.
- Never modify or delete a game/SU file except project-owned `CampaignCompletion*.asi`, the project-owned `CampaignCompletion` configuration directory, and the already authorized guarded exact replacement of `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
- Do not replace or remove any authorized external file while `S4_Main` or `Settlers United` is running.
- Preserve `research/backups/settlers-united/Plugin_SU.zip.original` with SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265` exactly.
- Write diagnostic output only below `F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics`; the packaged `CaptureTraceRoot` remains empty.
- Use only public S4ModApi observations; do not add an internal victory Hook, process patch, Lua write, game-data write, completion decision, completion storage, or marker rendering.
- Never dereference `LPS4GUIDRAWBLTPARAMS` text, tooltip, or extra-tooltip pointers.
- Keep the settlement feature limit at `256`, the window at `2,500 ms`, and the decision output exactly `diagnostic-result=calibration-only`.

## File Structure

- `src/victory/LaunchOrigin.h`: owns the private navigation-context state alongside the pending launch snapshot.
- `src/victory/LaunchOrigin.cpp`: applies authoritative context transitions and context-gated sibling-page rules.
- `tests/LaunchOriginTests.cpp`: encodes the exact live offline sequence plus online, random, campaign, load, and explicit-tab regressions.
- `src/runtime/S4Listeners.h`: declares the common settlement deadline operation.
- `src/runtime/S4Listeners.cpp`: calls and implements that operation from Tick and UI frames without changing probe ownership.
- `tests/RuntimePolicyTests.cpp`: statically proves both callback routes and preserves forbidden-behavior constraints.
- `docs/research/phase-3a-victory-ui-calibration-report.md`: records the invalid first sample, correction CI, frozen artifact, guarded redeployment, and replacement live samples.

---

### Task 1: Context-gated launch-origin classification

**Files:**
- Modify: `tests/LaunchOriginTests.cpp`
- Modify: `src/victory/LaunchOrigin.h`
- Modify: `src/victory/LaunchOrigin.cpp`

**Interfaces:**
- Consumes: `ObservePage(DWORD, uint64_t)`, `ObserveListKind(FixedMapListKind, uint64_t)`, and `ConsumeMapInit(uint64_t)`.
- Produces: private `NavigationContext { Unknown, SinglePlayer, OnlineMultiplayer }`; public `LaunchOriginTracker` signatures remain unchanged.

- [ ] **Step 1: Add exact failing origin regressions**

Add tests that feed the observed page sequence and require the following results:

```cpp
LaunchOriginTracker liveOffline;
for (const DWORD page : {
         S4_SCREEN_MAINMENU,
         S4_SCREEN_SINGLEPLAYER,
         S4_SCREEN_SINGLEPLAYER_CAMPAIGN,
         S4_SCREEN_SINGLEPLAYER_MAPSELECT_SINGLE,
         S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI,
         S4_SCREEN_SINGLEPLAYER_MAPSELECT_USER,
         S4_SCREEN_SINGLEPLAYER_LOBBY,
         S4_SCREEN_MULTIPLAYER_LOBBY}) {
    liveOffline.ObservePage(page, 100u);
}
const auto liveOrigin = liveOffline.ConsumeMapInit(200u);
Require(liveOrigin.source == LaunchSource::SinglePlayerMap &&
            liveOrigin.eligibility == SessionEligibility::Eligible,
        "live offline siblings remain eligible single player");

LaunchOriginTracker trueOnline;
trueOnline.ObservePage(S4_SCREEN_MULTIPLAYER, 100u);
trueOnline.ObservePage(S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI, 110u);
trueOnline.ObservePage(S4_SCREEN_MULTIPLAYER_LOBBY, 120u);
Require(trueOnline.ConsumeMapInit(200u).eligibility ==
            SessionEligibility::ExcludedOnlineMultiplayer,
        "authoritative multiplayer context excludes online sessions");
```

Also update the existing random, explicit fixed-tab, campaign, and load tests so they first establish the appropriate authoritative context. Assert that sibling fixed pages cannot clear `ExcludedRandomMap`, an explicit calibrated tab click refines the generic fixed source, a campaign without a later fixed lobby stays eligible, and multiplayer load is excluded directly.

- [ ] **Step 2: Push the tests and prove RED in Win32 CI**

Run the available local source checks:

```bash
git diff --check
git grep -n "GameDefaultGameEndCheck\|VICTORY_CONDITION_CHECK" -- src
```

Commit only the test change and push `main`:

```bash
git add tests/LaunchOriginTests.cpp
git commit -m "test: reproduce live launch origin misclassification"
git push origin main
```

Trigger or observe `build-debug-asi`; expected result is a failed CTest job whose message includes `live offline siblings remain eligible single player`.

- [ ] **Step 3: Add the minimal navigation context**

Add this private state to `LaunchOriginTracker`:

```cpp
enum class NavigationContext {
    Unknown,
    SinglePlayer,
    OnlineMultiplayer,
};

NavigationContext context_ = NavigationContext::Unknown;
```

Implement these rules in `ObservePage`:

```cpp
if (page == S4_SCREEN_MAINMENU) {
    ObserveBack();
    return;
}
if (page == S4_SCREEN_SINGLEPLAYER) {
    context_ = NavigationContext::SinglePlayer;
    Set(Origin(LaunchSource::SinglePlayerMap,
               SessionEligibility::Eligible), nowMs);
    return;
}
if (page == S4_SCREEN_MULTIPLAYER) {
    context_ = NavigationContext::OnlineMultiplayer;
    Set(Origin(LaunchSource::OnlineMultiplayer,
               SessionEligibility::ExcludedOnlineMultiplayer), nowMs);
    return;
}
if (IsCampaignPage(page)) {
    context_ = NavigationContext::SinglePlayer;
    Set(Origin(LaunchSource::Campaign,
               SessionEligibility::Eligible), nowMs);
    return;
}

switch (page) {
    case S4_SCREEN_SINGLEPLAYER_MAPSELECT_SINGLE:
    case S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI:
    case S4_SCREEN_SINGLEPLAYER_MAPSELECT_USER:
    case S4_SCREEN_SINGLEPLAYER_LOBBY:
        if (context_ == NavigationContext::SinglePlayer) {
            Set(Origin(LaunchSource::SinglePlayerMap,
                       SessionEligibility::Eligible), nowMs);
        }
        break;
    case S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM:
        if (context_ == NavigationContext::SinglePlayer) {
            Set(Origin(LaunchSource::RandomMap,
                       SessionEligibility::ExcludedRandomMap), nowMs);
        }
        break;
    case S4_SCREEN_MULTIPLAYER_MAPSELECT_MULTI:
    case S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM:
    case S4_SCREEN_MULTIPLAYER_MAPSELECT_USER:
    case S4_SCREEN_MULTIPLAYER_LOBBY:
        if (context_ == NavigationContext::OnlineMultiplayer) {
            Set(Origin(LaunchSource::OnlineMultiplayer,
                       SessionEligibility::ExcludedOnlineMultiplayer), nowMs);
        }
        break;
    case S4_SCREEN_LOADGAME:
        ObserveBack();
        break;
    case S4_SCREEN_LOADGAME_CAMPAIGN:
        Set(Origin(LaunchSource::LoadCampaign,
                   SessionEligibility::Eligible), nowMs);
        break;
    case S4_SCREEN_LOADGAME_MAP:
        Set(Origin(LaunchSource::LoadMapUnresolved,
                   SessionEligibility::Unknown), nowMs);
        break;
    case S4_SCREEN_LOADGAME_MULTIPLAYER:
        Set(Origin(LaunchSource::LoadOnlineMultiplayer,
                   SessionEligibility::ExcludedOnlineMultiplayer), nowMs);
        break;
    default:
        break;
}
```

In `ObserveListKind`, keep the early return for a disabled tracker or authoritative online exclusion. Otherwise set `context_ = NavigationContext::SinglePlayer`, clear `current_` and `observedAtMs_`, and retain the existing switch that maps `Single`, `Multiplayer`, and `Custom` to the three specific fixed-list sources. Add `context_ = NavigationContext::Unknown` wherever `ObserveBack`, `ConsumeMapInit`, and `Disable` clear the pending snapshot.

- [ ] **Step 4: Verify GREEN and commit**

Inspect the exact state transitions and run source checks:

```bash
git diff --check
rg -n "NavigationContext|S4_SCREEN_MULTIPLAYER_LOBBY|S4_SCREEN_SINGLEPLAYER_LOBBY" src/victory tests/LaunchOriginTests.cpp
```

Commit and push:

```bash
git add src/victory/LaunchOrigin.h src/victory/LaunchOrigin.cpp
git commit -m "fix: gate launch origin by navigation context"
git push origin main
```

Expected Win32 CI result: all origin tests pass, including the exact offline sequence and true-online control.

---

### Task 2: Advance settlement completion from public UI frames

**Files:**
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`

**Interfaces:**
- Consumes: `SettlementUiProbe::FinishIfDue(uint64_t)` and existing `Phase3Trace` channels.
- Produces: private `void FinishSettlementIfDue(std::uint64_t nowMs);`; public listener interfaces remain unchanged.

- [ ] **Step 1: Add a failing runtime-routing policy test**

Parse `S4Listeners.h/.cpp` and require:

```cpp
const auto listenerHeader = ReadText(sourceRoot / "src" / "runtime" /
                                     "S4Listeners.h");
Require(listenerHeader.find(
            "void FinishSettlementIfDue(std::uint64_t nowMs);") !=
            std::string::npos,
        "listener declares common settlement deadline operation");

const auto uiBegin = listeners.find("void S4Listeners::ObserveUiFrame(");
const auto mapBegin = listeners.find("void S4Listeners::ObserveMapInit(");
const auto tickBegin = listeners.find("void S4Listeners::ObserveTick(");
const auto mouseBegin = listeners.find("void S4Listeners::ObserveMouse(");
Require(uiBegin < mapBegin &&
            listeners.substr(uiBegin, mapBegin - uiBegin)
                    .find("FinishSettlementIfDue(now);") != std::string::npos,
        "UI frames advance the settlement deadline");
Require(tickBegin < mouseBegin &&
            listeners.substr(tickBegin, mouseBegin - tickBegin)
                    .find("FinishSettlementIfDue(now);") != std::string::npos,
        "non-delayed Tick retains settlement deadline fallback");
```

Count production-source occurrences of `GetLocalPlayer()` and require exactly one. Retain the existing conditional `HasPlayerLost`, GUI text-pointer, calibration-only, and forbidden internal-end-check assertions.

- [ ] **Step 2: Push the routing test and prove RED in Win32 CI**

```bash
git add tests/RuntimePolicyTests.cpp
git commit -m "test: require UI frame settlement deadline progress"
git push origin main
```

Expected CI failure: `listener declares common settlement deadline operation`.

- [ ] **Step 3: Extract and route the one-shot finalizer**

Move the existing `FinishIfDue` result handling from `ObserveTick` into:

```cpp
void S4Listeners::FinishSettlementIfDue(std::uint64_t nowMs) {
    if (api_ == nullptr || settlement_ == nullptr || phase3Trace_ == nullptr) {
        return;
    }
    const auto capture = settlement_->FinishIfDue(nowMs);
    if (!capture.has_value()) {
        return;
    }

    const DWORD localPlayer = api_->GetLocalPlayer();
    phase3Trace_->Write(
        Phase3TraceChannel::SettlementUi,
        localPlayer == 0u ? "local-player-status=unavailable"
                          : "local-player-status=available");
    if (localPlayer != 0u) {
        const BOOL lost = api_->HasPlayerLost(localPlayer);
        phase3Trace_->Write(Phase3TraceChannel::SettlementUi,
                            lost != FALSE ? "local-player-lost=1"
                                          : "local-player-lost=0");
    }

    std::ostringstream summary;
    summary << "settlement-capture=session-" << capture->sessionId
            << ";features=" << capture->features.size()
            << ";truncated=" << (capture->truncated ? 1 : 0);
    phase3Trace_->Write(Phase3TraceChannel::SettlementUi, summary.str());
    for (const auto& feature : capture->features) {
        phase3Trace_->Write(Phase3TraceChannel::SettlementUi,
                            SettlementFeatureRecord(feature));
    }
    phase3Trace_->Write(Phase3TraceChannel::Decision,
                        "diagnostic-result=calibration-only");
    if (logger_ != nullptr) {
        logger_->Write(LogLevel::Info,
                       "settlement UI calibration captured");
    }
}
```

Call `FinishSettlementIfDue(now);` in `ObserveUiFrame` immediately after page `34` can start `SettlementUiProbe`, before any early return caused by page-window logging. Call it in the existing non-delayed `ObserveTick` after bounded SU identity work. Both callers already hold `mutex_`; the helper must not lock it again.

- [ ] **Step 4: Verify GREEN and commit**

```bash
git diff --check
rg -n "FinishSettlementIfDue|GetLocalPlayer|HasPlayerLost|tooltipText|tooltipExtraText" src/runtime tests/RuntimePolicyTests.cpp
```

Commit and push:

```bash
git add src/runtime/S4Listeners.h src/runtime/S4Listeners.cpp
git commit -m "fix: finish settlement capture from UI frames"
git push origin main
```

Expected Win32 CI result: all CTest and public-calibration policy checks pass; `GetLocalPlayer()` has one production call site and `HasPlayerLost` remains conditional on a nonzero player ID.

---

### Task 3: Freeze the corrected Win32 artifact

**Files:**
- Modify: `docs/research/phase-3a-victory-ui-calibration-report.md`
- Generate, never commit: `artifacts/phase-3a-ci/<run-id>/...`

**Interfaces:**
- Consumes: one final correction commit containing Tasks 1 and 2.
- Produces: two clean workflow runs and one frozen PE32 package with exact hashes.

- [ ] **Step 1: Run the complete repository policy suite twice**

For the same final correction commit, require two clean `build-debug-asi` runs. Each run must pass archive integration, Win32 configure/build, public-calibration policy, all forbidden-behavior fixtures, CTest, packaging, PE32 machine `0x014c`, and exact two-entry package layout.

- [ ] **Step 2: Freeze and independently verify the later artifact**

Download the later `CampaignCompletionDebug-Win32` artifact below `artifacts/phase-3a-ci/<run-id>/`. Verify the GitHub artifact digest and record SHA-256 for the downloaded artifact, `CampaignCompletionDebug.zip`, embedded `Plugins/CampaignCompletionDebug.asi`, and `CampaignCompletion/CampaignCompletionDebug.ini`. Require the INI to contain exactly one empty `CaptureTraceRoot=` line.

- [ ] **Step 3: Update the pre-live report and commit**

Record the correction commit, workflow/run/job/artifact IDs, artifact digest, file hashes, PE machine, package entries, RED evidence from Tasks 1 and 2, and both GREEN runs. Mark the original Aeneas voluntary-exit attempt invalid because its origin and settlement channels were unusable.

```bash
git add docs/research/phase-3a-victory-ui-calibration-report.md
git commit -m "docs: freeze phase 3A calibration corrections"
git push origin main
```

---

### Task 4: Guarded redeployment to the correct configuration directory

**Files:**
- Read frozen artifact: `artifacts/phase-3a-ci/<run-id>/.../CampaignCompletionDebug.asi`
- Authorized replace: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`
- Authorized write: `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.ini`
- Authorized remove: `C:\Program Files\Settlers United\CampaignCompletion\CampaignCompletionDebug.ini`
- Create only below project: `artifacts/phase-3-victory-diagnostics`

**Interfaces:**
- Consumes: frozen ASI/INI, immutable archive backup, explicit existing deployment authorization, and a user-closed game.
- Produces: verified archive metadata and one live INI differing from frozen only by `CaptureTraceRoot`.

- [ ] **Step 1: Stop at the external-write gate**

Ask the user to close the game and Settlers United normally. Read process state without terminating anything. Continue only when neither `S4_Main` nor `Settlers United` is running. Reverify the immutable original hash, installed archive hash, frozen ASI hash, and absence of `Plugin_SU.zip.campaigncompletion.tmp`.

- [ ] **Step 2: Prepare and verify the live INI in project staging**

Copy the frozen INI into an ignored project staging directory and replace only:

```ini
CaptureTraceRoot=F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics
```

Normalize line endings for comparison and prove this is the only textual difference. Refuse deployment if any policy field or second `CaptureTraceRoot` differs.

- [ ] **Step 3: Perform only authorized external writes**

Run `tools/install_settlers_united_artifact.ps1` with the frozen ASI. Then create the authorized game `CampaignCompletion` directory if needed and atomically replace only `CampaignCompletionDebug.ini` with the verified staged live INI. Remove only the inert project-owned SU-side `CampaignCompletionDebug.ini`; remove its parent directory only when empty. Do not touch any other game/SU file.

- [ ] **Step 4: Verify deployment before launch**

Require:

- immutable original backup hash unchanged;
- all seven non-target original ZIP entries unchanged;
- exactly one `Plugins/CampaignCompletionDebug.asi` entry with the frozen hash;
- correct game-side INI hash and configured project trace root;
- inert SU-side INI absent;
- no archive temporary sibling;
- no write outside the three authorized targets.

Report `archivePath`, `originalSha256`, `patchedSha256`, `patchedSize`, `embeddedAsiSha256`, `installedIniPath`, and `installedIniSha256` before asking the user to launch.

---

### Task 5: Recollect the live control matrix

**Files:**
- Create only below project: `artifacts/phase-3-victory-diagnostics/<pid-session>/...`
- Modify after evidence: `docs/research/phase-3a-victory-ui-calibration-report.md`

**Interfaces:**
- Consumes: corrected deployed build and user-driven navigation/gameplay.
- Produces: trustworthy calibration-only control samples; no victory classifier or completion record.

- [ ] **Step 1: Repeat voluntary exit first**

Ask the user to enter Aeneas and voluntarily end the game, then remain on the settlement screen for at least 3 seconds. Accept the sample only if the new project trace root contains a matching live PID/session with confirmed SU path, eligible `single-player-map` origin, a nonempty bounded settlement capture, local-player status, and exactly one `diagnostic-result=calibration-only`.

- [ ] **Step 2: Collect settlement outcomes one at a time**

After voluntary exit passes, ask separately for normal victory and normal defeat. Verify each session is bounded, one-shot, responsive, and calibration-only. Compare numeric GUI signatures and `HasPlayerLost` observations without deciding completion yet.

- [ ] **Step 3: Collect origin/load controls one at a time**

Collect Single Player Maps, offline single-player Multiplayer Maps, Custom Maps, campaign, random map, true online multiplayer, ordinary map load, campaign load, and multiplayer load. Require explicit tab clicks for the three fixed lists. Ordinary map load remains `load-map-unresolved/unknown` until independent public evidence identifies its source; never infer it from display text or path naming.

- [ ] **Step 4: Stop cleanly and report the evidence gate**

Use only the existing controlled stop request after asking the user. Verify listener removal and trace flush, and ask the user to confirm the game remains responsive. Update the report with exact sample hashes and either `GO-TO-PHASE-3B` or `EVIDENCE-GAP`, with the concrete public evidence supporting that result.

- [ ] **Step 5: Commit the completed calibration report**

```bash
git add docs/research/phase-3a-victory-ui-calibration-report.md
git commit -m "docs: record corrected phase 3A live controls"
git push origin main
```
