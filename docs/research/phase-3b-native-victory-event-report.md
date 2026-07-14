# Phase 3B Native Terminal Event Calibration Report

## Scope and source evidence

- Final source commit: `4f601e09c8aaa1361b50a26b70c616e9c459a5d6`.
- Approved executable identity: `S4_Main.exe` version `2.50.1516.0`, SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Read-only disassembly of that exact executable established terminal event ID `609`, with `wParam=1` for the local alliance winning and `wParam=0` for losing. The same event paths exist for a live terminal result and for loading an already-terminal state.
- The admitted native layout is fixed to engine pointer slot RVA `0x0106B11C`, register operation RVA `0x0005A8A0`, and unregister operation RVA `0x0005A990`. Admission additionally verifies executable sections, bounded live memory, fixed instruction prefixes, and both relocated engine-slot operands.
- The event object view is exactly five Win32 words: vtable, event ID, wParam, lParam, and game tick. The subscriber copies only the four numeric event fields.

## No-code-patch architecture

- The plugin uses the game's existing register and unregister operations. It does not replace a call, function, pointer, IAT entry, vtable entry, or instruction byte.
- There is no memory scan and no use of `WriteProcessMemory`, `VirtualProtect`, a Hook/patch framework, Lua mutation, game-data mutation, or a victory-condition call.
- The registered object's first and only virtual slot is `OnEvent`. Every callback path returns `false`, so event propagation remains transparent.
- Registration and unregistration are serviced only by public S4ModApi UI-frame or non-delayed Tick callbacks on the game thread. The native callback performs no file I/O, allocation, Lua/API call, or retained game-pointer storage.
- Controlled stop requests native detachment first. Public listeners, the trace, the registrar, and the process-lifetime subscriber remain alive until detachment is confirmed. A five-second timeout records `controlled-stop-flush=failed` and retries later without unloading registered state.
- The build remains calibration-only: `InternalVictoryHook=0`, `HookCount=0`, `CodePatchBytes=0`, `CompletionDetection=0`, `CompletionStorage=0`, and `CompletionMarkers=0`.

## RED/GREEN evidence

| Task | RED evidence | GREEN evidence |
| --- | --- | --- |
| Bounded event probe and trace | run `29263536059`, job `86862747820`, missing `VictoryEventProbe` production source | run `29263781693`, job `86863603056`, success |
| Exact native admission | run `29264016029`, job `86864424833`, missing `NativeEventAdmission.cpp` | run `29264280021`, job `86865322706`, success |
| Transparent subscriber lifecycle | run `29264470333`, job `86865992058`, missing registration source | run `29264669727`, job `86866676217`, exposed a missing ASI probe link; corrected run `29264850096`, job `86867300924`, success |
| Runtime and detach ordering | run `29265150008`, job `86868309659`, CTest failed exactly on the missing Phase 3B runtime mode | run `29265357408`, job `86869022085`, success |
| Configuration and zero-patch policy | run `29265511944`, job `86869544563`, CTest failed exactly on missing Phase 3B INI fields | run `29265721243`, job `86870251265`, success |

All GREEN runs passed the Settlers United archive integration test, Win32 configure/build with `/W4 /WX`, source-closure policy verification, adversarial mutation fixtures, CTest, packaging, PE32/layout verification, and artifact upload.

## Independent final verification

Two separate workflow dispatches ran the unchanged final commit:

- Run `29265931645`, job `86870997722`: success. Artifact ID `8285420653`; GitHub digest `sha256:1ec7000e593592c168a13ed6b76d2b3c1033d2a0eac2fe731b2f84248188818b`; GitHub size `546609` bytes.
- Run `29265935761`, job `86871010625`: success. Artifact ID `8285423680`; GitHub digest `sha256:e6057520d3d189ee5e7ff395b6cfe47e1e81e6c3844a30a51e02b290a93968cd`; GitHub size `546604` bytes.

The later artifact was downloaded and frozen below ignored project directory `artifacts/phase-3b-ci/29265935761-attempt-1/`:

- Downloaded GitHub artifact ZIP SHA-256 `e6057520d3d189ee5e7ff395b6cfe47e1e81e6c3844a30a51e02b290a93968cd`, size `546604` bytes. Its hash exactly equals the GitHub digest.
- Inner `CampaignCompletionDebug.zip` SHA-256 `824706df3272590117bf4ba0806ad50ef68b92f79c008f1b21bed5099e8bd9d9`, size `547026` bytes.
- Frozen ASI SHA-256 `d0b6f498198eb9f924f16434bd2eb91dde129437be34571303cd111d58465036`, size `1654784` bytes; PE signature `PE\0\0`, machine `0x014c`, optional-header magic `0x010b`.
- Frozen INI SHA-256 `3ddd6420c4680a455bff2bd1a272f9724b5e04abe0d25d9368e7eb35e805c2b8`, size `619` bytes.
- The inner package contains exactly `Plugins/CampaignCompletionDebug.asi` and `CampaignCompletion/CampaignCompletionDebug.ini`.
- The corrected INI contains exactly one `Version=0.3.2`, `DiagnosticMode=NativeVictoryEventCalibration`, `NativeEventSubscription=1`, `NativeTerminalEventId=609`, and empty `CaptureTraceRoot=`. Every zero-Hook, zero-patch, zero-write, and zero-completion field occurs exactly once.

## Live sample correction: registered-handler ordering

The first voluntary-exit sample and the UBO(test) AI-resign victory sample both
left `native-event.trace` with only `native-subscription=attached`, even though
the victory sample visibly opened the game's win/loss dialog. Reverse-source
and exact-executable analysis established the ordering cause:

- `IEventEngine::RegisterHandle` inserts with `push_front`.
- The diagnostic subscriber originally attached while the game was still in
  the main-menu lifecycle.
- `CStateGame::InitGUI` later creates and registers `CGuiEventHandler`, placing
  it ahead of the diagnostic subscriber.
- `CGuiEventHandler::OnEvent` handles event `609`, opens dialog `72`, passes
  `m_wParam` to `UpdateGuiDlgWinLoss`, and returns true. `m_wParam == 1` selects
  translation `GUI_MSG_WIN`; all other values select `GUI_MSG_LOST`.
- The event engine stops iterating registered handles after a handler returns
  true, so the lower-priority diagnostic subscriber cannot observe `609`.

Version `0.3.2` keeps the same native registration API and installs no process
hook or code patch. Each MapInit arms one reorder. On the first non-delayed Tick
where `S4_SCREEN_INGAME` is active and the subscriber is attached, the runtime
unregisters and immediately registers the same transparent subscriber once.
This moves it to the list front; its callback still always returns false, so the
game's own GUI handler continues to receive the event and display the normal
win/loss dialog. The trace records either
`native-subscription=reinserted-front` or the explicit failure form, and never
retries the reorder repeatedly within the same map session.

## Guarded deployment and remaining live controls

- The user explicitly approved the `0.3.2` deployment. Fresh read-only process checks immediately before deployment found neither `S4_Main` nor Settlers United, and no process was terminated.
- Deployment used the fixed-hash elevated wrapper and the existing guarded archive installer. Only the authorized `Plugin_SU.zip`, game-side `CampaignCompletionDebug.ini`, and project deployment evidence were written.
- The corrected source commit `2253cd54022cd42d93533123954d86bb860a45a5` passed two independent complete Win32 workflows: runs `29268716027` and `29268737175`.
- Installed archive: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`; SHA-256 `175d580f78e80ff0994d738123b6da80f365e9a4362d9ec8ec5ae344575c1355`; size `1702061` bytes.
- The immutable original archive backup remains SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`, size `1176944` bytes.
- Complete ZIP verification found eight entries: all seven original non-target entries remained byte-identical to the immutable backup, and exactly one `Plugins/CampaignCompletionDebug.asi` had frozen SHA-256 `fca972726376c5c29f36413faf5f410b3d7e3ebb73f8de22581474cd2d7c8cd0`.
- Live configuration: `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.ini`; SHA-256 `3c445e5949ac26028498c85dc6081993b083c554fd93f011986777ef4c0b8136`.
- Logical-line comparison proved the live INI differs from the frozen INI only by setting `CaptureTraceRoot=F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics`. The trace root exists and is not a reparse point.
- The inert SU-side INI and both authorized temporary siblings are absent after deployment.

### Live control 1: voluntary exit

- Accepted session: `phase-3-session-35588`, map-init session `1`.
- The loaded `CampaignCompletionDebug.asi` matched frozen SHA-256 `d0b6f498198eb9f924f16434bd2eb91dde129437be34571303cd111d58465036`, and the exact executable remained compatible.
- Native evidence contained exactly `native-subscription=attached` and no `native-event=` record, so voluntary exit produced no terminal event `609`.
- Origin was `single-player-map` / `eligible`; SU identity confirmed `Map\Singleplayer\Aeneas.map`.
- Settlement evidence recorded an available local player, `local-player-lost=0`, one bounded capture, and exactly one `diagnostic-result=calibration-only`.
- `S4_Main` remained responsive on the settlement page after evidence collection.

### Live control 2: UBO(test) AI-resign local victory

- Accepted process/session: PID `10764`, `phase-3-session-10764`, map-init session `1`.
- The loaded `CampaignCompletionDebug.asi` matched the deployed `0.3.2` SHA-256 `fca972726376c5c29f36413faf5f410b3d7e3ebb73f8de22581474cd2d7c8cd0`.
- Native evidence recorded `native-subscription=attached` followed by exactly one `native-subscription=reinserted-front`.
- The visible in-game victory dialog coincided with exactly one terminal record: `event-id=609`, `local-result=won`, `wparam=1`, `lparam=0`, `game-tick=145`, duplicate count `0`.
- Origin was `single-player-map` / `eligible`; SU identity confirmed `Map\Singleplayer\Aeneas.map`.
- `S4_Main` remained responsive while the victory dialog was visible. The after-game page was not required to establish this event result.

### Live control 3: Custom Map local defeat

- Accepted process/session: PID `12768`, `phase-3-session-12768`, map-init session `1`.
- The Custom Map identity was confirmed as `Antares`, `Map\User\Antares.map`.
- Launch origin remained `single-player-map` / `eligible`, confirming that an offline Custom Map is not excluded from completion eligibility.
- Native evidence recorded `native-subscription=attached`, exactly one `native-subscription=reinserted-front`, then exactly one terminal record: `event-id=609`, `local-result=lost`, `wparam=0`, `lparam=0`, `game-tick=145`, duplicate count `0`.
- `S4_Main` remained responsive while the defeat dialog was visible.

Collect the remaining controls one at a time and verify game responsiveness after each:

1. Load-before-victory: recovered origin and identity plus event `609`, `wParam=1`.
2. Controlled stop: native detachment precedes public-listener removal and trace flush succeeds.

Completion storage and completed-level markers remain disabled until these live controls pass.

## Load-session attribution correction and deployment

The first read-save victory control captured event `609` with
`local-result=won`, `wparam=1`, and duplicate count `0`, but it was rejected as
a complete control because origin and identity recovery failed closed. The
load selector exposes pages `31`, `32`, and `33` as simultaneous siblings, so
page-order observation incorrectly retained the online-multiplayer sibling.
MapInit also preceded the new LuaOpen generation, causing the identity session
to bind the previous generation and report `stale-generation`.

Read-only reverse-source evidence maps the load-type controls to the game's
native state transitions: control `2390` selects single-player saves, `2391`
selects multiplayer saves, and `2399` selects campaign saves. Commit
`1cb983cc975e9cf45e24e5527aeb1230ef5350c1` therefore treats the explicit
control as stronger evidence than sibling load pages. A Lua generation may
replace the MapInit binding only before the first read attempt; replacement
after reading begins remains terminal `stale-generation`.

- RED workflow `29272108934` failed at Build on the missing explicit-load
  observer introduced by the regression test.
- The first GREEN candidate workflow `29272334386` passed Build and policy but
  exposed an old stale-generation test whose pre-read assumption contradicted
  the observed load ordering.
- Final workflow `29272707020` passed Win32 `/W4 /WX` Build, policy and mutation
  checks, all CTest cases, packaging, PE32/package verification, and artifact
  upload.
- Downloaded inner package SHA-256:
  `bc19772faadcdaa167d357b53f22d2b916fca6afa7d9a2b992c8d96774ea7ead`.
  It contains exactly the ASI and frozen INI.
- Deployed ASI SHA-256:
  `59497cc3211112b76a267b546f986ab4be5da0159416b6d05751daaa395cc15f`;
  the file is PE32 Intel 80386.
- Installed `Plugin_SU.zip` SHA-256:
  `9169a76671a9af1ae4eade2df29f6f152bc870933788912c2adef267de36d225`;
  size `1702433` bytes.
- The immutable original archive remains SHA-256
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`,
  size `1176944` bytes, with its original timestamp unchanged.
- Independent entry verification found all seven original non-target entries
  byte-identical and exactly one authorized CampaignCompletion ASI entry.
- The live INI remains SHA-256
  `3c445e5949ac26028498c85dc6081993b083c554fd93f011986777ef4c0b8136`.
  No archive temporary sibling or inert Settlers United-side INI remains.

The corrected deployment still has completion detection, storage, and markers
disabled. Repeat the read-save victory control to validate recovered origin,
confirmed map identity, and event `609` in the same session.

## 2026-07-14 random-map recording-policy deployment

- Approved source commit:
  `a73f123f5c10f021114430fa1726c3a05713078c`; GitHub Actions run
  `29276366546` completed successfully.
- GitHub artifact ID `8289546752` has SHA-256
  `3d67f68b6bb7ee137a6a68324723b2ac06dd58d060c7fd2658328f2b9a2a036d`.
  The independently downloaded artifact matched this digest exactly.
- The package contained exactly
  `Plugins/CampaignCompletionDebug.asi` and
  `CampaignCompletion/CampaignCompletionDebug.ini`.
- Deployed ASI SHA-256:
  `2d451df8d3ebb4fde2bafbf99318bf1cf9a0890324c1b82f913d715e1878447e`;
  it is PE32 Intel 80386.
- Installed `Plugin_SU.zip` SHA-256:
  `90a35a2e94a226b0bef904f1d5d73a780b0af1a3f04e3f8fbcd1139be6bfdfb6`;
  size `1703917` bytes.
- The immutable original archive remains SHA-256
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`,
  size `1176944` bytes, with timestamp
  `2026-05-10T08:38:14.0000000Z` unchanged.
- Independent ZIP inspection found all seven original non-target entries
  byte-identical, eight total entries, and exactly one
  `Plugins/CampaignCompletionDebug.asi` entry whose hash matches the frozen
  ASI.
- The live game-side INI SHA-256 is
  `88737dad30a5981085ec31bb797fe793ef3242644f15f66e812a7c075b6385c1`.
  It identifies version `0.3.3`, uses only the project trace root, and retains
  `CompletionDetection=0`, `CompletionStorage=0`, and
  `CompletionMarkers=0`.
- Fresh process checks before and after deployment found neither `S4_Main` nor
  Settlers United running. No authorized archive or INI temporary sibling
  remained.

This diagnostic build classifies fresh and loaded random maps as recordable
but marker-hidden policy inputs. It still does not persist completion or draw
the marker.

## 2026-07-14 loaded-random live validation

- The user launched the deployed `0.3.3` build and loaded the paired random
  save. The active process was PID `10700` and remained responsive throughout
  inspection.
- The synchronized game-side `CampaignCompletionDebug.asi` SHA-256 was
  `2d451df8d3ebb4fde2bafbf99318bf1cf9a0890324c1b82f913d715e1878447e`,
  exactly equal to the frozen CI ASI. The game trace independently listed the
  module as loaded.
- MapInit session `1` initially recorded
  `load-map-unresolved/unknown`.
- The same session returned successful SU values with both name and relative
  identifier equal to `RD_LCGSDR30`, followed by
  `identity-association=confirmed`.
- The bounded post-identity record was exactly
  `origin-refinement=session-1;source-random-map;eligibility-eligible;ui-hidden`.
- The native event subscriber attached and was reinserted at the front after
  game initialization. No terminal event was expected or emitted during this
  source-classification sample.
- UI-frame and GUI-element callbacks continued after the refinement, providing
  an independent responsiveness signal.

This sample passes the loaded-random source-policy gate: a random-map save is
recordable, but its future completed-level marker is suppressed.

## 2026-07-14 loaded-fixed negative control

- Without restarting PID `10700`, the user returned to the menu and loaded the
  paired fixed/custom save.
- MapInit session `2` initially recorded
  `load-map-unresolved/unknown`.
- The same session returned successful SU values: name
  `Battle of the Gods`, relative identifier
  `Map\User\Battle of the Gods.map`, and
  `identity-association=confirmed`.
- The bounded post-identity record was exactly
  `origin-refinement=session-2;source-load-single-player-map;eligibility-eligible;ui-visible`.
- The random identifier rule therefore did not misclassify the ordinary
  game-relative path. The paired session `1` and session `2` observations
  exercise both sides of the same deployed classifier in one process.
- The native subscriber was reinserted for session `2`, and UI/GUI callbacks
  continued while PID `10700` remained responsive.

This sample passes the loaded-fixed negative-control gate: an ordinary loaded
map is recordable and remains eligible for its future completed-level marker.
