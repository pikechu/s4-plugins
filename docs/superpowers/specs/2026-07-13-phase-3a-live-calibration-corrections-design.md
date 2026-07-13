# Phase 3A Live Calibration Corrections Design

## Purpose

Correct three issues exposed by the first live voluntary-exit attempt so Phase 3A can collect trustworthy public-API evidence:

1. sibling UI pages must not turn an offline single-player launch into online multiplayer;
2. settlement capture must finish even when non-delayed Tick callbacks stop on the after-game screen;
3. the live calibration INI must be installed where the loaded ASI actually reads it.

The failed attempt remains diagnostic evidence only. It is not an exit, victory, or defeat control sample.

## Observed Evidence

The frozen `0.3.0` ASI was synchronized into the actual game directory and loaded with SHA-256 `5d5080965be4b0a95ce41ddfe9002e6a062f5192ea16663de9de9d267ec8b193`. The in-process log recorded a compatible executable, runtime start, map-init session `1`, and confirmed SU identity `Map\Singleplayer\Aeneas.map`.

The live page sequence before MapInit was:

```text
1
1,7
4,7,22,23,25
4,22,23,25
4,22,23,25,26,30
26,30
MapInit
```

Page `7` established Single Player. Page `26` was the single-player lobby. Page `30`, the multiplayer-lobby enum, was drawn as a sibling in the same offline flow. Treating every raw page `27` through `30` as authoritative online evidence therefore produced the incorrect trace:

```text
origin-source=online-multiplayer
origin-eligibility=excluded-online-multiplayer
```

After in-game pages were observed, page `34` remained active for minutes on the voluntary-exit settlement screen. No settlement trace was emitted. Public UI-frame callbacks continued, but the non-delayed Tick path used to call `FinishIfDue` did not advance there.

The ASI resolves configuration from the actual game directory because its loaded path is below `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\Plugins`. The active INI was therefore the old Phase 2.5 file below the sibling `CampaignCompletion` directory. The newly installed INI below `C:\Program Files\Settlers United` was inert.

## Scope

### In scope

- context-gated launch-origin classification using public UI observations;
- exact regression coverage for the observed offline and online page sequences;
- settlement deadline advancement from both public Tick and public UI-frame callbacks;
- one-shot settlement emission with the existing bounded numeric feature set;
- corrected deployment of the project-owned INI to the actual game directory;
- removal of the inert, project-owned INI below the Settlers United directory after the game is closed;
- a repeated voluntary-exit sample followed by victory, defeat, and load-origin samples.

### Out of scope

- deciding that any settlement is a victory;
- persisting completion or rendering markers;
- calling an internal game-end check;
- Lua writes, process patches, internal Hooks, save writes, or text-pointer reads;
- changing any non-project game or Settlers United file.

## Launch-Origin Context Model

`LaunchOriginTracker` will separate navigation context from the most specific launch source.

The navigation context has three states:

- `Unknown`;
- `SinglePlayer`;
- `OnlineMultiplayer`.

Authoritative context transitions are deliberately narrow:

- `S4_SCREEN_SINGLEPLAYER` enters `SinglePlayer` and establishes only the generic eligible `SinglePlayerMap` source;
- campaign selectors enter `SinglePlayer` and establish an eligible campaign source;
- `S4_SCREEN_MULTIPLAYER` enters `OnlineMultiplayer`;
- `S4_SCREEN_MAINMENU` clears the context and pending source;
- `S4_SCREEN_LOADGAME_MULTIPLAYER` directly produces an online exclusion;
- an explicit calibrated fixed-list tab click is the only evidence that may refine the generic fixed-map source to Single Player Maps, offline Multiplayer Maps, or Custom Maps.

Child and sibling page rules depend on that context:

- pages `27` through `30` produce an online exclusion only while the context is `OnlineMultiplayer`;
- pages `22`, `23`, `25`, and `26` may restore or preserve only generic eligible `SinglePlayerMap` while the context is `SinglePlayer`; because these pages are drawn together, none identifies a specific list;
- the single-player random selector produces `ExcludedRandomMap` while the context is `SinglePlayer`, and its exclusion cannot be downgraded by later sibling page callbacks;
- a fixed-map lobby observed after a transient campaign-selector sibling may replace the campaign source with generic `SinglePlayerMap`, matching the live sequence in which page `4` precedes pages `22`, `23`, `25`, and `26`;
- a campaign or campaign-load source remains eligible when no later fixed-map lobby is observed;
- ordinary map load remains `LoadMapUnresolved/Unknown` for live source-recovery evidence;
- online exclusion retains precedence once an authoritative online context exists.

The exact observed offline sequence must finish as eligible Single Player, even though page `30` appears. A true online sequence containing authoritative page `8` before pages `27` through `30` must finish as excluded online multiplayer.

MapInit still consumes the pending snapshot once and retains the 30-second lease. No map path or display text is used to infer network context.

## Settlement Deadline Driver

The existing `SettlementUiProbe` remains the sole owner of the 2.5-second window, deduplication, 256-feature cap, truncation flag, session ID, and one-shot completion state.

`S4Listeners` will extract one private operation, conceptually:

```text
FinishSettlementIfDue(now)
```

It will be called under the listener mutex from:

1. the existing non-delayed Tick path, after bounded SU identity work; and
2. every public UI-frame observation, after page `34` has had an opportunity to start the settlement capture.

Calling the same operation from two callback types does not create two outputs. `SettlementUiProbe::FinishIfDue` atomically makes the capture inactive when it returns a result, and `settlementStarted_` remains true for the map session. Later callbacks therefore see no due capture.

At the one successful finish:

- `GetLocalPlayer()` is called exactly once;
- `HasPlayerLost(localPlayer)` is called only for a nonzero player ID;
- the local-player state, capture summary, and bounded numeric features are written to `settlement-ui.trace`;
- `decision.trace` receives only `diagnostic-result=calibration-only`;
- GUI `text`, `tooltipText`, and `tooltipExtraText` are never dereferenced.

The operation remains inert when the trace is unavailable, the API is unavailable, the capture is not active, the deadline has not arrived, or the runtime is stopping.

## Configuration and Deployment

The packaged INI remains portable with an empty `CaptureTraceRoot`.

For live calibration, the only permitted difference is:

```ini
CaptureTraceRoot=F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics
```

After the user closes the game normally, guarded deployment will:

1. replace only `Plugins/CampaignCompletionDebug.asi` inside the authorized `Plugin_SU.zip` using the existing verified installer and immutable backup;
2. install the live INI at `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.ini`;
3. remove only the inert project-owned `C:\Program Files\Settlers United\CampaignCompletion\CampaignCompletionDebug.ini`, and remove its directory only if it is then empty;
4. preserve all other files and verify the archive, embedded ASI, correct INI, trace root, and absence of temporary siblings.

No running-game file will be replaced, removed, or hot-reloaded.

## Tests and Verification

### RED tests

- Feed the exact live offline sequence through `LaunchOriginTracker` and require an eligible single-player result.
- Feed a true online sequence beginning with `S4_SCREEN_MULTIPLAYER` and require online exclusion.
- Require random exclusion to survive sibling callbacks and an explicit fixed-list click to refine an offline source.
- Require runtime routing to call the common settlement deadline operation from both Tick and UI-frame paths.
- Preserve policy assertions for a single `GetLocalPlayer()` call site, conditional `HasPlayerLost`, no GUI text dereferences, no internal game-end behavior, and calibration-only decisions.

The first Win32 CI run must fail for the observed missing behavior before production code changes are made.

### GREEN verification

- Full Win32 build with `/W4 /WX /permissive-`;
- all CTest cases;
- public-calibration verifier and all forbidden-behavior fixtures;
- PE32 and exact package-layout checks;
- two clean successful runs for the final correction commit;
- frozen artifact digest and ASI/INI SHA-256 verification.

### Live acceptance

The repeated voluntary-exit run is accepted only when the new project trace root contains a session matching the live game PID with:

- an eligible offline source;
- confirmed SU map identity;
- a nonempty, bounded settlement capture after 2.5 seconds;
- local-player status;
- only `diagnostic-result=calibration-only` in the decision channel;
- no completion data or marker output;
- continued game responsiveness.

Only after that control passes will victory, defeat, online, random, offline Multiplayer Maps, and load-game controls be collected one at a time.

## Failure Handling

- Ambiguous navigation context yields `Unknown`; it never becomes eligible by guess.
- An unavailable or invalid trace root disables trace output without changing game behavior.
- A missing deadline callback produces no settlement conclusion.
- Failed deployment leaves the immutable original backup untouched and is reported before launch.
- Any mismatch in archive entries, ASI hash, INI normalization, process state, or trace policy stops the workflow.
