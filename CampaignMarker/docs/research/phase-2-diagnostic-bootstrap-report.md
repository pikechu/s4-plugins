# Phase 2 Diagnostic Bootstrap Report

Date: 2026-07-13  
Target: The Settlers IV History Edition through Settlers United  
Plugin: `CampaignCompletionDebug.asi`  
Source branch: `main`

## Result

The hook-free diagnostic bootstrap is operational on the target installation.
It loads as a 32-bit ASI, validates the exact executable, inventories the live
process, creates the S4ModApi 2.3.2 interface, and receives public map, UI, mouse,
and GUI-element callbacks without crashing the game.

This build does not detect victory, persist completion state, create completion
JSON, or draw completion markers. No internal game hook was installed.

## Static and CI Evidence

- GitHub Actions run `29222845147` passed the Win32 logger build and tests.
- Run `29222910649` provided the expected red inventory build before production
  inventory code existed.
- Run `29223227476` passed module enumeration, SHA-256, version, and compatibility
  tests.
- Run `29223287013` provided the expected red runtime-policy build.
- Run `29223441980` passed the hook-free runtime tests and linked the Win32 ASI.
- Run `29223505857` provided the expected red packaging check.
- Run `29223615401` passed configure, build, tests, PE32 verification, exact ZIP
  layout verification, and artifact upload.
- The downloaded workflow artifact digest was
  `9868ce747da7e40ae900fb84805d3aa80f67da7998f69652e8247ca596c7789b`.
- The inner ASI SHA-256 was
  `bc27ee4d1b02f977f0e2b9c3b9b03483536b8c08a55e19ba8e20e28eef1a1698`.
- The PE machine field was `0x014c` (`IMAGE_FILE_MACHINE_I386`).
- The package contained exactly:
  - `Plugins/CampaignCompletionDebug.asi`
  - `CampaignCompletion/CampaignCompletionDebug.ini`

## Deployment Finding

Copying the diagnostic ASI into `Plugins` before launching Settlers United was
not sufficient. On both observed launches, Settlers United's pre-launch plugin
synchronization removed `CampaignCompletionDebug.asi` before `S4_Main.exe`
started. The second observation recorded restoration at `12:35:35.825`; the
formal game process then started at `12:36:03`.

Windows Defender operational events and threat detections contained no matching
entry. A one-shot watcher that restored only the explicitly authorized
`CampaignCompletionDebug.asi` after synchronization allowed the ordinary ASI
loader to load it. No Settlers United file or other game file was changed.

This is a development deployment issue, not a plugin entry-point failure. A
repeatable release installation mechanism must integrate with or run after
Settlers United's plugin synchronization.

## Runtime Evidence

The log recorded:

- bootstrap version `0.2.0`, `hook-mode=public-only`;
- `S4_Main.exe` file version `2.50.1516.0`;
- accepted executable SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4ModApi.dll` version `2.3.2.0` and SHA-256
  `ed111491b1a6bd2822ccd21fec31adedfb35b6bf311cd568d9ff0e6e2193126f`;
- the diagnostic ASI and all baseline Settlers United/HD Patch ASI modules;
- `executable compatibility=compatible`;
- `diagnostic runtime started; no internal hooks installed`;
- stable main-menu callbacks for page `1`;
- no `[ERROR]` or `[WARN]` record during the test;
- a responsive `S4_Main.exe` process throughout menu and map testing.

Two bootstrap blocks appeared about one second apart. The first preceded the
formal process start time and lacked later Settlers United modules; the second
contained the complete module set. Evidence is consistent with Settlers United's
short-lived startup process followed by the formal game process, rather than a
duplicate runtime inside the surviving process. A future build should include
PID in every bootstrap header to make this distinction explicit.

## User-Assisted Menu Coverage

The user visited all three single-player map categories plus the following
campaign/menu families:

- Single Player Maps, Multiplayer Maps, and Custom Maps;
- Great Crusades;
- Tutorial;
- Add On: Trojan, Roman, Mayan, Viking, and Settlement campaigns;
- Mission CD: Roman, Viking, Mayan, and Conflict and Settlement campaigns;
- ordinary campaigns and Dark Tribe;
- the main menu before and after traversal.

Observed page values were `1`, `2`, `3`, `4`, `5`, `7`, and `11` through `21`.
Representative callback evidence included:

- page `1`: element `2536`, rectangle `550,310,160,30`;
- page `7`: element `2404`, rectangle `450,360,200,125`;
- page `11`: element `90`, rectangle `512,516,43,65`;
- page `15`: elements around values `77`/`81` and the common back rectangle;
- page `19`: element `1940`, rectangle `512,517,43,65`;
- page `20`: element `2037`, rectangle `512,516,43,65`;
- page `21`: element `2080`, rectangle `512,516,43,65`.

Although the user visited all three map-list types, dedicated map-list page
values `22` through `25` did not surface. The current shared UI-frame callback
calls `IsCurrentlyOnScreen` in ascending order and therefore records a lower
active parent page before a nested page. Per-page callback thunks are required
to preserve the page value associated with each `AddUIFrameListener`
registration.

## Fixed-Map Test

The user loaded one fixed single-player map, remained in game without winning,
and returned normally. Evidence showed:

- selection/lobby page `26`;
- transition page `37`;
- exactly one `map-init observed` record;
- in-game page `36` with ongoing UI and GUI-element callbacks;
- after-game/exit page `34`;
- return to main-menu page `1`;
- no completion JSON;
- no victory or completion claim;
- no error, warning, crash, or forced exit.

## Protected-Input Verification

`sha256sum -c research/evidence/manifest.sha256` reported `OK` for all 12
protected inputs: the three Settlers United binaries, five baseline ASI files,
`S4ModApi.dll`, `S4_Main.exe`, `S4_MainR.pdb`, and `ddraw.dll`.

Only the authorized diagnostic ASI, configuration, and plugin-generated log were
created or replaced.

## Remaining Risks and Decision

Verified:

- exact executable fail-closed policy;
- Win32 build/package identity;
- in-process module inventory;
- S4ModApi interface creation;
- public listener registration and callback stability;
- fixed-map initialization without a false completion record;
- protected input integrity.

Not yet runtime-verified:

- controlled `Stop()` and reverse listener removal inside the live game;
- precise nested map-list page attribution;
- a persistent deployment method that survives Settlers United synchronization.

Decision: the public diagnostic approach is **GO**, but direct internal-probe
work is **NO-GO** until controlled listener cleanup is exercised and per-page
callback attribution is corrected. The next safe increment should address those
two diagnostic gaps and the post-synchronization deployment workflow; it should
not add completion detection or internal hooks yet.
