# Phase 7.1 main-menu hotkey fix candidate audit

Date: 2026-07-18 (Asia/Shanghai)

## Decision

Phase 7.1 is **GREEN for the audited offline `0.13.1` candidate**.

Live acceptance of the deployed `0.13.0` candidate proved that the exact
audited ASI loaded and that the completion-manager UI thread and persistence
runtime started, but `Ctrl+Shift+M` did not open the window. Read-only module
and log capture identified the cause: the hotkey was sampled from the public
game-tick callback, while the main menu produced public page-1 UI-frame
callbacks but no game ticks.

The `0.13.1` correction samples the same edge-triggered hotkey from the public
main-menu UI-frame callback. It preserves the page-1, `S4_SCREEN_MAINMENU`,
and not-in-game gate. Repeated menu availability notifications are suppressed
with an atomic state transition.

This audit does not authorize deployment or any completion-database mutation.

## Frozen source

- branch: `main`
- commit: `a1d4dc2f497263922195f53abd3f11fcfa4a137e`
- subject: `fix: sample manager hotkey on menu frames`
- version: `0.13.1`
- independent `AIGoodCrashRepair` and `MusicDuplicatedRepair` work: excluded
- `docs/handoffs/`: excluded

No identity or classification behavior changed. Fixed/random classification
continues to use only the exact confirmed same-session `identity.relative`;
display names, save names, and `identity.name` remain inadmissible. Exact
relative identifiers beginning with `RD_` remain random.

## Live diagnosis evidence

The deployed `0.13.0` session loaded:

- `CampaignCompletionDebug.asi` SHA-256:
  `0e057c2497da12c66f5562b37b85b4ec98ef5d0c595b797add4586d1f33d8367`
- `PileChainRepair.asi` SHA-256:
  `7d17f6de446d43a22bfe30647585ba1bd2b5a4acc3781cb5b9eb24f4ead33493`

The runtime log recorded:

- `CampaignCompletionDebug bootstrap version=0.13.0`
- exact compatible executable admission
- writable six-record completion-store load
- `diagnostic runtime started`
- repeated `ui-pages active=1 primary=1`

There was no completion-manager open request. Source inspection then confirmed
that the only key sampling occurred in `ObserveTick`, which is not invoked on
the main menu.

## Local validation

- changed `S4Listeners.cpp`: Win32 MinGW compile with warnings as errors
- changed `CompletionManagerWindow.cpp`: Win32 MinGW compile with warnings as
  errors
- changed `DiagnosticRuntime.cpp`: Win32 MinGW compile with warnings as errors
- changed runtime-policy test: Win32 MinGW compile with warnings as errors
- focused completion-manager hotkey executable: exit `0`
- source-only zero-process-patch/read-only-Lua policy: success
- `git diff --check`: success

The runtime-policy regression requires:

- `GetAsyncKeyState` sampling to be inside `ObserveUiFrame`;
- the sample to precede `ObserveMapInit`;
- no key sampling inside the `ObserveTick` function body;
- an exact `page == S4_SCREEN_MAINMENU` gate.

## Windows CI

- workflow: `build-debug-asi`
- run: `29643530073`
- job: `88077823804`
- result: `success`
- commit: `a1d4dc2f497263922195f53abd3f11fcfa4a137e`

Every authoritative step succeeded:

1. Settlers United archive integration tests
2. Win32 configure and MSVC build with warnings as errors
3. PE32/source zero-process-patch and read-only-Lua verification
4. all 11 forbidden-behavior mutation fixtures
5. complete CTest suite
6. package generation
7. PE32 and exact package-layout verification
8. artifact upload

## Artifact audit

- artifact ID: `8429355374`
- artifact name: `CampaignCompletionDebug-Win32`
- artifact API size: `313942` bytes
- GitHub digest:
  `c8636ffff138f28bb4d6bb7abd14ffa086475dafb3f8c67e5044a3cbe86aaf1a`
- independently downloaded outer ZIP SHA-256:
  `c8636ffff138f28bb4d6bb7abd14ffa086475dafb3f8c67e5044a3cbe86aaf1a`
- inner deployment ZIP SHA-256:
  `b0be46595c1940c8237c0a38ca04519a8a67b57557343366110a7c9b19d2f958`
- ASI SHA-256:
  `da76dd7e620f55ec705d21a2836deb495f84bde6344096bdd48ed49835d6fd22`
- packaged INI SHA-256:
  `0e04d705c5dc9ffcd97cb22c0bf9f419e282b253cffedc3600f8ea4afca2b0fe`

The outer artifact contains exactly `CampaignCompletionDebug.zip`. The inner
ZIP contains exactly:

- `Plugins/CampaignCompletionDebug.asi`
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`

The ASI is a PE32 Intel 80386 DLL with exactly one export:
`CampaignCompletionDebugStop`. The packaged INI is byte-equivalent to the
reviewed source after CRLF normalization and declares version `0.13.1`, mode
`Phase7ClassifiedCompletionManager`, and `OpenHotkey=Ctrl+Shift+M`.

## Deployment boundary

The audited `0.13.1` artifact remains undeployed. The currently running game
has `0.13.0` loaded and must be closed normally by the user. Settlers United
must also be closed. Deployment requires a fresh explicit approval and must:

- use the current combined `Plugin_SU.zip` as the guarded rollback baseline;
- replace only `Plugins/CampaignCompletionDebug.asi`;
- preserve `PileChainRepair.asi` and every other ZIP entry byte-for-byte;
- verify the final archive and embedded ASI hashes before starting SU.

Initial live acceptance is limited to opening the window from the main menu.
No checkbox may be changed until the user separately approves a specific
database-mutation test.
