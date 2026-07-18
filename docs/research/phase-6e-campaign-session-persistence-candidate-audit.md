# Phase 6E campaign session persistence candidate audit

Date: 2026-07-18 (Asia/Shanghai)

## Decision

The Phase 6E candidate is **artifact-audited / not deployed**.

It is eligible for a separately approved guarded deployment and one bounded
live campaign-victory transaction. This audit does not authorize deployment,
game launch, live database access, or a victory.

## Source checkpoint

- commit:
  `9ab07f12356000d8e77d34b9afc684454d95e202`
- subject: `feat: restore exact campaign victory persistence`
- branch: `main`
- the local RED commit was squashed into this single GREEN checkpoint
- `docs/handoffs/` remained untracked and was not committed

The candidate adds one bounded `CampaignSessionAdmission` and reconnects the
previously accepted native event `609`, completion admission, atomic store,
bounded worker, and marker-index pipeline. It does not add a Hook, process
patch, synthetic input, game-end call, Lua write, game-data write, save write,
or campaign-progress write.

Campaign promotion requires the same nonzero session, an exact immutable
descriptor match, an exact descriptor key, and exact equality between the
descriptor, association, and confirmed `identity.relative`. `identity.name`,
visible names, save names, and arbitrary `RD` strings are not classification
inputs. An exact confirmed relative beginning `RD_` remains random.

## Local constructed evidence

- the complete Win32 cross-compiled test executable exited successfully
- changed runtime translation units cross-compiled successfully
- `git diff --check` passed
- `verify_no_process_patch.ps1 -SourceOnly` passed:
  `Verified zero process patches and read-only Lua bridge.`

Constructed coverage includes representatives from all six descriptor groups,
the Trojan misleading-origin regression, session/key/case/geometry/online/RD
fail-closed cases, one-shot admission, and same-process campaign-index
publication only after a committed store transaction.

## Authoritative Windows CI

- workflow: `build-debug-asi`
- run: `29638011319`
- job: `88063638864`
- runner: `windows-2022`
- checked-out SHA:
  `9ab07f12356000d8e77d34b9afc684454d95e202`
- conclusion: `success`

Every workflow step completed successfully:

1. Settlers United archive integration
2. Win32 configure and MSVC build
3. PE32/source policy verification
4. all forbidden-behavior mutation fixtures
5. CTest (`100% tests passed, 0 tests failed`)
6. package creation
7. PE32 and exact two-file package-layout verification
8. artifact upload

The authoritative policy output was:

`Verified PE32 completion persistence ASI, zero process patches, and read-only Lua bridge.`

## Artifact identity

- GitHub artifact ID: `8427738152`
- artifact name: `CampaignCompletionDebug-Win32`
- artifact size: `286335` bytes
- outer artifact SHA-256:
  `7692a66056235c0bc88ad3f9d812aaa641cd80c71a98a5f61d778f88561deb2d`
- inner deployment archive SHA-256:
  `6bc685d8f058425f2e07cb6b97182ede73abfb8bcbdba6780da9ab44cef633cd`

The deployment archive contains exactly:

- `Plugins/CampaignCompletionDebug.asi`
  - SHA-256:
    `4e0b08fb85ed3e0004e7c6b3ff84fc9e4f0c040cd63598bb9f03b90b764f763d`
  - PE32 / Intel i386 DLL
  - exactly one exported stop entry:
    `CampaignCompletionDebugStop`
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`
  - SHA-256:
    `de830132d2982228920da16e825750b8261c487770467ff088aef7e2fcdc32bc`
  - version `0.12.0`
  - mode `Phase6ECampaignSessionPersistence`
  - native event subscription enabled for exact event `609`
  - transactional completion storage enabled
  - Hook count, code-patch bytes, game-end calls, Lua writes, and game-data
    writes remain zero

Binary strings identify:

- `version=0.12.0`
- `mode=phase-6e-campaign-session-persistence`
- `storage-writes=transactional`
- `native-event=609-exact`
- `internal-menu-adapter=read-only`

The source-closure verifier rejects direct use of `VirtualProtect`,
`WriteProcessMemory`, patch libraries, game-end checks, and Lua-write APIs.
The MSVC-linked PE imports `VirtualProtect` through linked runtime support, but
no admitted project source references or invokes it; the authoritative
source-closure and mutation checks passed.

## Remaining live boundary

A future live acceptance needs fresh explicit approval for:

1. guarded deployment of the exact audited inner archive above;
2. startup preflight proving no database change;
3. one predeclared campaign mission and one normal local victory;
4. exactly one committed record (or a predeclared duplicate);
5. immediate marker refresh on return without hover;
6. controlled close and database/process postflight.

No family-by-family victory repetition is required. The shared integration
needs one live transaction; all 107 descriptor rows remain covered by static
catalog and constructed tests.

## Guarded deployment

The user subsequently approved deployment of the exact audited Phase 6E
candidate. Fresh preflight independently confirmed:

- protected-process count `0`;
- installed Phase 6D archive SHA-256
  `f584df8f64b6abb90945ae4617bb3bc43098b2eb597655524408c82b6217b836`;
- live Phase 6D INI SHA-256
  `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`;
- database main and backup hashes, sizes, and timestamps exactly matched the
  Phase 6D postflight;
- immutable original and installer metadata matched their fixed expected
  hashes;
- archive, INI, and database temporary siblings were absent.

The fixed-input elevated transaction replaced only the project ASI inside
`Plugin_SU.zip` and the project-owned live INI. It created the immutable
pre-deployment snapshot
`research/backups/campaign-completion/2026-07-18-pre-v0.12.0-phase6e-persistence`.
Any exception after replacement was configured to restore the preceding
archive, INI, and installer metadata.

Independent post-deployment verification recorded:

| Item | Deployed identity |
| --- | --- |
| protected processes | `0` |
| installed `Plugin_SU.zip` | `1,444,503` bytes; SHA-256 `b209572f45f447759cecbac227a35d6513f50d032febdf37f33408807aa4f1e4` |
| embedded project ASI | SHA-256 `4e0b08fb85ed3e0004e7c6b3ff84fc9e4f0c040cd63598bb9f03b90b764f763d` |
| live project INI | `1,455` bytes; SHA-256 `de830132d2982228920da16e825750b8261c487770467ff088aef7e2fcdc32bc` |
| database main | `1,260` bytes; timestamp `2026-07-16T05:41:41.3482563Z`; SHA-256 `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| database backup | `951` bytes; timestamp `2026-07-14T11:13:02.1756072Z`; SHA-256 `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |
| immutable original | SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265` |
| installer metadata | SHA-256 `9a8413ed320fd75cf5a4048ff035e47777e3411d92e78bbfab2e52a31d67cb3c` |

All authorized temporary siblings remained absent. The machine-readable result
is `artifacts/phase6e-persistence-9ab07f1/deployment-result.json`.

Deployment is complete, but the live transaction is not. The next boundary is
a user-driven startup to the main menu, followed by a no-write startup
preflight. Mission selection and one normal victory remain separately bounded
by the explicit live-acceptance procedure above.
