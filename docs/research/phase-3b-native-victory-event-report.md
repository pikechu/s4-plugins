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
- The INI contains exactly one `Version=0.3.1`, `DiagnosticMode=NativeVictoryEventCalibration`, `NativeEventSubscription=1`, `NativeTerminalEventId=609`, and empty `CaptureTraceRoot=`. Every zero-Hook, zero-patch, zero-write, and zero-completion field occurs exactly once.

## Deployment gate and remaining live controls

No game or Settlers United file was modified during Phase 3B implementation, CI, or artifact freezing. A final read-only process check found no matching game/SU process, but deployment still requires the user's explicit confirmation that the game is closed. The guarded backup/hash/archive workflow must be used afterward; no automatic deployment is authorized by this report.

After a later approved deployment, collect these controls one at a time and verify game responsiveness after each:

1. Eligible-map voluntary exit: no event `609` record.
2. Normal local victory: event `609`, `wParam=1`, local result `won`.
3. Normal local defeat: event `609`, `wParam=0`, local result `lost`.
4. Load-before-victory: recovered origin and identity plus event `609`, `wParam=1`.
5. Controlled stop: native detachment precedes public-listener removal and trace flush succeeds.

Completion storage and completed-level markers remain disabled until these live controls pass.
