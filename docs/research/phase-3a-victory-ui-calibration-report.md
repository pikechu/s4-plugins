# Phase 3A Public Victory UI Calibration Report

## Build and frozen artifact evidence

- Source commit: `a3cda38ec334ebf7da7275294f00973569ad7c37`.
- GitHub Actions workflow: `build-debug-asi`; run: `29256442353`.
- Successful jobs: `86837923430` (attempt 1) and `86838335883` (attempt 2).
- Both attempts passed the Settlers United archive integration test, Win32 configure and build, public-calibration policy verifier, all forbidden-behavior fixtures, CTest, package layout and PE32 checks, and artifact upload.
- Frozen artifact: ID `8281509763`; GitHub digest `sha256:513045c5eba9aa5f8739b62f2d56417306b989c63ec66d9064243e6de55dd450`; downloaded size `534309` bytes.
- Downloaded artifact SHA-256: `513045c5eba9aa5f8739b62f2d56417306b989c63ec66d9064243e6de55dd450`, exactly equal to the GitHub digest.
- Inner diagnostic package SHA-256: `a1240dd6eab61fbca4777b4ef3b040d53991617966d82f185ba4b3d51f285f89`; size `534545` bytes.
- Frozen ASI SHA-256: `5d5080965be4b0a95ce41ddfe9002e6a062f5192ea16663de9de9d267ec8b193`; size `1625088` bytes; PE machine `0x014c` (`IMAGE_FILE_MACHINE_I386`).
- Frozen INI SHA-256: `25f418f3f7de191dffb525a8fccd142df647a279c70594d7b4a321409b97036b`; size `552` bytes.
- The inner package contains exactly `Plugins/CampaignCompletionDebug.asi` and `CampaignCompletion/CampaignCompletionDebug.ini`.
- The packaged INI identifies version `0.3.0`, `DiagnosticMode=VictoryUiCalibration`, public settlement UI and launch/load-origin tracking, and zero internal victory Hook, code patch, Lua write, game-data write, completion detection, storage, or marker behavior. Its `CaptureTraceRoot` is empty.
- The frozen files are retained below the ignored project directory `artifacts/phase-3a-ci/29256442353-attempt-2/`.

## Enforced calibration boundary

- Production sources linked into the ASI are scanned transitively through local headers.
- The verifier rejects internal/default game-end checks, the Lua victory-condition handler, the internal victory-screen class, Lua mutation APIs, process-write/page-protection APIs, and Hook/patch frameworks.
- CI mutation fixtures prove rejection of `GameDefaultGameEndCheck`, `DefaultGameEndCheck`, `VICTORY_CONDITION_CHECK`, `CStateVictoryScreen`, `lua_setglobal`, `lua_settable`, `WriteProcessMemory`, `VirtualProtect`, and `hlib::CallPatch`.
- Runtime traces are admitted only below a pre-existing non-reparse project root. Each process creates a unique directory with four fixed channels and bounded allowlisted records.
- GUI capture stores numeric public callback fields only. It does not dereference GUI text, tooltip text, or extra tooltip text pointers.
- Offline single-player Multiplayer Maps remain eligible evidence. Online multiplayer and random-map sources are explicitly excluded; ordinary map loads remain `load-map-unresolved` until live evidence resolves their source.

## Initial guarded deployment evidence

- Deployment occurred only after explicit user approval and a fresh check that `S4_Main` and Settlers United were not running.
- Installed archive: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`; SHA-256 `c39ca63c4a38636870495999025e00468107942e4d8e228a5979c387be396a17`; size `1688885` bytes.
- Immutable original archive backup SHA-256 remains `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`; size `1176944` bytes.
- The installed ZIP passed a complete integrity test. All seven original non-target entries matched the immutable backup byte-for-byte, the installed archive contained eight file entries, and `Plugins/CampaignCompletionDebug.asi` occurred exactly once.
- Embedded ASI SHA-256: `5d5080965be4b0a95ce41ddfe9002e6a062f5192ea16663de9de9d267ec8b193`, exactly equal to the frozen ASI.
- The first live INI was written to `C:\Program Files\Settlers United\CampaignCompletion\CampaignCompletionDebug.ini`; SHA-256 `b727c1cbf36a3005a3771cc305b253e8f3e5a4ac7625fbcd39604e78cbd95118`; size `604` bytes.
- A normalized textual comparison proved that this INI differed from the frozen INI only by setting `CaptureTraceRoot=F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics`.
- Live inspection subsequently proved this SU-side INI was inert. Because the loaded ASI was synchronized below the actual game `Plugins` directory, it resolved configuration from `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.ini`, where the older Phase 2.5 INI remained active.
- The trace root existed as a normal directory, not a reparse point. No authorized archive or INI temporary sibling remained after deployment.
- A post-deployment process check again found no game or Settlers United process.

## Invalid first live sample and root causes

- The first voluntary-exit Aeneas attempt proved that the synchronized `0.3.0` ASI loaded and that SU identity confirmed `Map\Singleplayer\Aeneas.map`.
- Its exact pre-MapInit public page sequence included `1`, `7`, the simultaneously drawn pages `4,22,23,25`, then `26,30`. The old tracker treated sibling page `30` as authoritative and incorrectly emitted `origin-source=online-multiplayer` with `origin-eligibility=excluded-online-multiplayer`.
- Page `34` remained active on the voluntary-exit settlement screen, but no settlement or decision record was emitted. UI-frame callbacks continued while the non-delayed Tick path that alone called `FinishIfDue` did not advance there.
- Because the actual game-side Phase 2.5 INI was read, Phase 3 traces were written below the old `dist/phase-2-5/runtime` root rather than the intended Phase 3 project root.
- This attempt is diagnostic fault evidence only. It is not accepted as a voluntary-exit, victory, or defeat control sample.

## Live-correction TDD and frozen artifact evidence

- Correction source commit: `4fc1c913d675bf768de89ed8111584a407229e35`.
- Launch-origin RED: run `29259074240`, job `86847153756`, commit `573187f6239c246c77b110eb337b7637cb8f9cff`. Win32 build and all policy checks passed; CTest failed exactly with `FAIL: live offline siblings remain eligible single player`.
- Launch-origin GREEN: run `29259328623`, job `86848033013`, commit `1a851a6843e6e77be832f099afe19c9ac6e00971`. Every workflow step passed.
- Settlement-routing RED: run `29259521915`, job `86848716794`, commit `4d7ff3254e86233f7dd26e71fc292e1190754b50`. Win32 build and all policy checks passed; CTest failed exactly with `FAIL: listener declares common settlement deadline operation`.
- Final correction validation: run `29259755088`; successful jobs `86849544921` (attempt 1) and `86850057030` (attempt 2). Both attempts passed archive integration, Win32 configure/build, public-calibration policy, every forbidden-behavior fixture, CTest, packaging, PE32/layout verification, and artifact upload.
- Attempt-2 frozen artifact: ID `8282903608`; GitHub digest `sha256:97b99c5dae2554f4151c8aa338fcd5598984233f3af7c3d6d8c8f1f2914876cb`; downloaded size `534313` bytes. The independently downloaded artifact SHA-256 is identical to that digest.
- Inner diagnostic package SHA-256: `a5a12b5991c8aa6ac31824b2916f0c74b9a1fab21e0b3a6056d6e345e565267f`; size `534549` bytes.
- Corrected frozen ASI SHA-256: `a8dc3af496fc61ce2dc9cb05845095993793298a23db351cbf60b2dd1bb0ea43`; size `1625600` bytes; PE machine `0x014c` (`IMAGE_FILE_MACHINE_I386`).
- Corrected frozen INI SHA-256: `25f418f3f7de191dffb525a8fccd142df647a279c70594d7b4a321409b97036b`; size `552` bytes; it contains exactly one empty `CaptureTraceRoot=` line.
- The inner package contains exactly `CampaignCompletion/CampaignCompletionDebug.ini` and `Plugins/CampaignCompletionDebug.asi`.
- The frozen files are retained below the ignored project directory `artifacts/phase-3a-ci/29259755088-attempt-2/`.
- The corrected tracker gates sibling pages through `Unknown`, `SinglePlayer`, and `OnlineMultiplayer` navigation context. The settlement probe retains one-shot ownership while a common deadline operation is called from both public UI-frame and non-delayed Tick callbacks.

## Pre-live scope statement

This calibration build does not decide victory, persist completion, or render markers. It records bounded public GUI and source evidence only.

## Corrected guarded redeployment evidence

- The user closed the game normally. Fresh process checks before and after deployment found neither `S4_Main` nor Settlers United running; no process was terminated.
- The first non-elevated installer attempt was denied while creating the authorized temporary archive sibling. It did not change the archive, immutable backup, metadata, or any game file, and left no temporary sibling.
- The fixed-hash elevated wrapper then used the existing guarded installer. Installed archive: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`; SHA-256 `8d7e0fa774fe633a820c595f05da001c615a2db0de9ee65921c87cec7400dc4a`; size `1688928` bytes.
- Immutable original archive backup SHA-256 remains `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`; size `1176944` bytes.
- ZIP compressed-data verification passed. All seven original non-target file entries matched the immutable backup byte-for-byte; the installed archive contains eight file entries and exactly one `Plugins/CampaignCompletionDebug.asi`.
- Embedded ASI SHA-256 is `a8dc3af496fc61ce2dc9cb05845095993793298a23db351cbf60b2dd1bb0ea43`, exactly equal to the corrected frozen ASI.
- Live INI was installed at the actual configuration path `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.ini`; SHA-256 `b727c1cbf36a3005a3771cc305b253e8f3e5a4ac7625fbcd39604e78cbd95118`.
- Normalized comparison proves the live INI differs from the frozen INI only by `CaptureTraceRoot=F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics`.
- The inert project-owned `C:\Program Files\Settlers United\CampaignCompletion\CampaignCompletionDebug.ini` was removed only after its hash matched the expected live INI. Its directory was removed only because it was empty.
- Neither `Plugin_SU.zip.campaigncompletion.tmp` nor `CampaignCompletionDebug.ini.campaigncompletion.tmp` remained after deployment.

The corrected artifact is deployed and ready for a replacement voluntary-exit control sample.

## Live evidence gate

The following evidence is still required before Phase 3A can produce a GO decision:

1. Eligible-map voluntary-exit settlement sample.
2. Eligible-map victory settlement sample.
3. Eligible-map defeat settlement sample.
4. New-game source samples for campaign, fixed maps, offline Multiplayer Maps, random maps, and online multiplayer.
5. Load-game source samples sufficient to distinguish eligible ordinary saves from random-map and online-multiplayer saves.
6. Controlled-stop evidence showing all listeners removed, trace flush success, and normal game responsiveness.

No victory/defeat/exit classifier will be implemented until those samples provide stable, reviewable signatures.

## 2026-07-14 ordinary-load and random-map identity resolution

- Corrected load-page control evidence maps control `2390` to an ordinary
  single-player save, `2391` to an excluded online-multiplayer save, and
  `2399` to an eligible campaign save. The correction is present in source
  commit `1cb983cc975e9cf45e24e5527aeb1230ef5350c1` and passed GitHub Actions
  run `29272707020`.
- A loaded fixed/custom positive sample confirmed the SU identity
  `Battle of the Gods` and relative identifier
  `Map\User\Battle of the Gods.map`.
- A paired fresh-random and loaded-random sample confirmed the same SU name
  and relative identifier in both sessions: `RD_LCGSDR30`.
- Byte-identical project evidence copies were retained below
  `research/evidence/save-samples/2026-07-14-load-origin/`. The random sample
  SHA-256 is
  `cbb3213d7a600b1f9e40070f0570a0f67c19f9cd221ce7b15f653d80c7077fbb`;
  the fixed/custom sample SHA-256 is
  `c45f75e3c2b91c52c38f289e0156957c0f168d12e475c8a0c90d0f4b7c82e03f`.
  The original save files were not modified.
- Reverse-source inspection established that
  `CRandomMaps::IsRandomMapFileName` treats a case-sensitive `RD_` prefix,
  a complete `[ ... ]` wrapper, or a complete `< ... >` wrapper as a random
  identifier. `CRandomMaps::GenerateRandomMapFileName` creates the `RD_`
  form, and normal game map loading consults the same predicate.
- The approved policy supersedes the earlier Phase 3A random-map exclusion:
  random-map victories are recordable, but their future completed-level UI
  marker is hidden. Source/type, recording eligibility, and marker visibility
  are separate policy axes. True online multiplayer remains excluded.
- The implementation uses only the validated result of
  `SU.Game.GetMapNameRelativePath()` and a pure replica of the game's bounded
  identifier rule. It adds no save parser, internal game call, process-memory
  probe, Hook, or patch.
- Post-MapInit refinement accepts only a confirmed identity whose session ID
  equals the active MapInit session. Invalid, stale, conflicting, unavailable,
  and mismatched-session evidence remains fail-closed.
- Diagnostic build `0.3.3` still has `CompletionDetection=0`,
  `CompletionStorage=0`, and `CompletionMarkers=0`. This change validates
  source and policy classification only; it does not yet persist a victory or
  render any marker.
