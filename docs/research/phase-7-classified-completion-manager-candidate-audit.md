# Phase 7 classified completion manager candidate audit

Date: 2026-07-18 (Asia/Shanghai)

## Decision

Phase 7A is **GREEN for the audited offline candidate**.

The candidate implements a native classified completion-manager window and
conflict-safe manual completion edits. It has passed local constructed tests,
the complete Windows CI policy/build/test/package gate, and an independent
artifact hash and package-content audit.

This decision does not authorize deployment or live database edits. No game or
Settlers United process was started or controlled, and no live completion
database was read or written during Phase 7 development and audit.

## Frozen source

- branch: `main`
- feature commit:
  `fdd5f0cb9aa28de2c1f5b6aefcf15e51bb548759`
- subject: `feat: add classified completion manager`
- independent `PileChainRepair` work: excluded from the commit
- `docs/handoffs/`: excluded from the commit

## Implemented behavior

- `Ctrl+Shift+M` is edge-triggered and opens the manager only from the public
  main menu.
- The left tree classifies all admitted campaign missions into Add-on,
  Mission CD, New World, New World 2, Original, and Dark Tribe families,
  including the known race/subcampaign subdivisions.
- The map tree classifies exact installed files from
  `Map\Singleplayer`, `Map\Multiplayer`, and `Map\User`.
- The right list shows the current checkbox state and exact relative identity.
- Random-map history is visible but read-only and marker-hidden.
- Check/uncheck changes are submitted as one bounded transaction.
- A monotonic store revision rejects stale-window apply attempts before file
  I/O, preventing a concurrent automatic victory from being overwritten.
- Successful apply publishes both fixed-map and campaign marker indexes.
- New manual records use the exact source `manual-manager`; all other new
  record-source spellings remain rejected.

The implementation never uses display names, save names, or `identity.name` to
classify a map. Random classification continues to depend only on the exact
confirmed relative identity and the existing case-sensitive RD rules.

## Local validation

The locally constructed Win32 test executable completed successfully,
including all pre-existing persistence, RD, campaign descriptor, renderer,
and mutation-policy tests plus the new Phase 7 tests.

Focused tests additionally proved:

- all 107 admitted campaign descriptors and installed fixed maps join by
  stable ID rather than display text;
- normalized map identity collisions fail closed;
- exact directory roots and one-level `.map` enumeration are enforced;
- nested directories, reparse points, traversal-like names, and capacity
  overflow are not admitted;
- manual add/remove is one atomic replacement;
- no-op and stale-revision requests perform no transaction;
- failed replacement preserves the snapshot and revision;
- read-only store modes perform no write;
- random rows cannot enter a manual command;
- a manual command cannot forge the `native-event-609` source;
- the hotkey is main-menu gated and never repeats while held.

Additional local gates:

- `tools/verify_no_process_patch.ps1 -SourceOnly`: success
- `git diff --check`: success
- runtime policy test: success

## Windows CI

- workflow: `build-debug-asi`
- run: `29640064734`
- job: `88069006175`
- result: `success`
- feature commit:
  `fdd5f0cb9aa28de2c1f5b6aefcf15e51bb548759`

Every job step completed successfully:

1. Settlers United archive integration tests
2. Win32 configure
3. MSVC build with warnings as errors
4. PE32/source zero-process-patch and read-only Lua verification
5. all 11 forbidden-behavior mutation fixtures
6. complete CTest suite
7. packaging
8. PE32 and exact package-layout verification
9. artifact upload

## Artifact audit

- artifact ID: `8428350080`
- artifact name: `CampaignCompletionDebug-Win32`
- artifact size: `313755` bytes
- GitHub digest:
  `1232fc3c68896fb482134c819af44b1dfdfef1572cf01d4181948cd5dcb56d0c`
- independently downloaded outer ZIP SHA-256:
  `1232fc3c68896fb482134c819af44b1dfdfef1572cf01d4181948cd5dcb56d0c`
- inner deployment ZIP SHA-256:
  `7490e24268fd457d3a588b27607529226e9e36653159b77644fcaef35daab814`
- ASI SHA-256:
  `0e057c2497da12c66f5562b37b85b4ec98ef5d0c595b797add4586d1f33d8367`
- packaged INI SHA-256:
  `2caf2fa3aea55cb2e25e26ec7bcde28ec3f0e1beaf985c56897882e66f33dab7`

The outer artifact contains exactly one file:

- `CampaignCompletionDebug.zip`

The inner ZIP contains exactly:

- `Plugins/CampaignCompletionDebug.asi`
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`

The ASI is a PE32 Intel 80386 DLL and exports exactly one symbol:
`CampaignCompletionDebugStop`.

The packaged INI is byte-equivalent to the reviewed source after normalizing
the package's CRLF line endings. It declares version `0.13.0`, mode
`Phase7ClassifiedCompletionManager`, the exact `Ctrl+Shift+M` shortcut,
atomic manual apply, random-record read-only behavior, and revision conflict
protection.

## Permission boundary

The audited artifact remains undeployed. Deployment requires fresh explicit
user approval after both protected applications are closed and must use the
existing exact guarded backup/rollback installer. Live acceptance must begin
with viewing the classified window and may not change a checkbox until the
user separately approves a specific database mutation test.
