# Phase 6C.1 descriptor-family/session-gate candidate audit

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The implementation checkpoint
`8b27ce4f7493538154e42da93fc951b873002bc1` is **STATIC GO** for a guarded
Phase 6C.1 live diagnostic candidate. This is not deployment approval and is
not Phase 6C.1 live GO.

The candidate remains read-only. It does not enable completion storage,
completion markers, native victory events, internal menu access, hooks,
patches, Lua writes, game-data writes, database access, or save access.

## Corrected contract

- Page 16 controls `1903/1904` are admitted only through the closed
  `event 0x1B60 -> kind 5 -> md_roman%01i` path family.
- The exact accepted relatives are `Map\Campaign\md_roman1.map` and
  `Map\Campaign\md_roman2.map`; `mcd2_*` is rejected.
- Pages 17–19, New World, and New World 2 remain evidence gaps and cannot arm
  a lease.
- Only one exact admitted page/control/rectangle with a labeled public
  snapshot can arm the 30-second lease.
- The next fresh nonzero MapInit session may bind when independent launch
  origin is unknown. Explicit online origin rejects and clears the lease.
- Exact confirmed same-session `identity.relative` remains mandatory.
  `identity.name`, display/save names, and `RD` text are ignored.

The frozen RVA/file-offset/byte/SHA manifest is recorded in
`phase-6c-1-descriptor-family-path-evidence.md`.

## Local gates

- `git diff --check`: PASS.
- `tools/verify_no_process_patch.ps1 -SourceOnly`: PASS.
- Win32 MinGW syntax compile of both changed campaign units and tests: PASS.
- Win32 link and direct Windows execution:
  - `CampaignLaunchAssociationTests`: PASS;
  - `CampaignDescriptorCatalogTests`: PASS;
  - `RuntimePolicyTests`: PASS.
- Exact installed `S4_Main.exe` SHA-256 remained
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.

MSVC `/W4 /WX`, the full test suite, mutation fixtures, packaging, PE32, and
export verification were deferred to the authoritative Windows workflow.

## Authoritative Windows CI

- Workflow: `build-debug-asi`.
- Run: `29490915089`.
- Job: `87596568585`.
- Commit: `8b27ce4f7493538154e42da93fc951b873002bc1`.
- Result: PASS.
- Started: `2026-07-16T10:28:01Z`.
- Completed: `2026-07-16T10:30:32Z`.

Every relevant step was successful, including Win32 configuration, MSVC
build, rendering-policy verification, all forbidden-behavior mutation
fixtures, tests, package creation, and `Verify PE32 and package contents`.

## Artifact audit

- Artifact ID: `8372403402`.
- Artifact name: `CampaignCompletionDebug-Win32`.
- GitHub-reported size: `240914` bytes.
- Downloaded workflow wrapper SHA-256:
  `9245332f3ae08718d19c8b7b33168419be350b622e2ac66150b076a451a9cfd6`.
- Candidate `CampaignCompletionDebug.zip` size: `240843` bytes.
- Candidate ZIP SHA-256:
  `b13ae9fc1fc006c734673d282ceac7bd864f1dd48f70cfd985fda8e238058859`.
- Packaged ASI size: `431616` bytes.
- Packaged ASI SHA-256:
  `342f2c97b49f2f507f006f64dbb10322050b77c460598b40f724862d034d457c`.
- Packaged INI size: `1180` bytes.
- Packaged INI SHA-256:
  `024c92062fbdd42703dfa0b30a0de2dc609d61fa075ba31abd4cadc35c899e7b`.

The packaged INI is semantically exact after the expected LF-to-CRLF package
normalization. Manual PE inspection independently confirmed `pei-i386`, PE32
optional-header magic `0x010B`, and exactly one
`CampaignCompletionDebugStop` export. The authoritative CI performed the full
`dumpbin` check.

## Boundary and next action

No game or Settlers United process was started, stopped, or inspected. No
installed archive, plugin file, database, backup, save, or campaign-progress
state was read or modified during implementation and audit. The untracked
`docs/handoffs/` directory was excluded from both commits and staging.

A future replacement of
`C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`
requires a fresh explicit deployment approval after the user has normally
closed both relevant processes. The efficient live batch remains limited to
Add-on Trojan 1 and page-16 Roman 1.

## Guarded deployment

The user subsequently confirmed both protected applications were closed and
explicitly approved deployment of this exact Phase 6C.1 candidate. A fresh
preflight confirmed:

- protected-process count `0`;
- prior installed archive size `1,398,739` bytes and SHA-256
  `058c4dc251b321d06f9a006af54a6b50b20857f010127cf8749a6fff96ef9a46`;
- prior live INI SHA-256
  `4e870e63d8cfeadfd38e90dedf26481acb34b5406a22f119017bcfa7a1b16850`;
- candidate ASI and INI exactly matched the audited hashes above;
- immutable original archive SHA-256 remained
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- database main/backup bytes and timestamps exactly matched the Phase 6C
  normal-shutdown postflight;
- no authorized temporary sibling existed.

Before mutation, the elevated fixed-hash script created and verified the full
rollback snapshot at
`research/backups/campaign-completion/2026-07-16-pre-v0.9.1-descriptor-family-session-gate`.
The established guarded installer then replaced only the project ASI archive
entry, and the project INI was replaced atomically in its existing directory.

Independent post-deployment verification recorded:

| Item | Result |
| --- | --- |
| installed `Plugin_SU.zip` | `1,399,996` bytes; SHA-256 `ac69b07181c2e832b750591921e1fe73308c2eff8910f6ce17068137b6894988` |
| embedded ASI | SHA-256 `342f2c97b49f2f507f006f64dbb10322050b77c460598b40f724862d034d457c`, exact candidate |
| live INI | `1,180` bytes; SHA-256 `024c92062fbdd42703dfa0b30a0de2dc609d61fa075ba31abd4cadc35c899e7b`, exact candidate |
| database main | `1,260` bytes; timestamp `2026-07-16T05:41:41.3482563Z`; SHA-256 `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| database backup | `951` bytes; timestamp `2026-07-14T11:13:02.1756072Z`; SHA-256 `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

The installed archive contains the same seven immutable original file entries
plus the project ASI. Its only changed/added file entry relative to the
immutable original is `Plugins/CampaignCompletionDebug.asi`. Installer metadata
matches both the installed archive and embedded ASI hashes. Protected-process
count remained zero, and the archive, INI, and database temporary siblings were
all absent. The machine-readable result is retained at
`artifacts/phase6c1-8b27ce4/deployment-result.json`.

Deployment is complete. Dynamic acceptance remains pending and is restricted
to the Add-on Trojan 1 and page-16 Roman 1 paths; no database, save, progress,
or marker authority follows from deployment.

## Live acceptance: GO

The user launched the deployed candidate and reached the main menu. The runtime
loaded the exact candidate ASI SHA-256 and exact approved executable, then
reported `addon=admitted`, `mdroman=admitted`, `original=admitted`,
`dark=admitted`, with both New World groups disabled. The read-only runtime
boundary remained active.

The efficient two-path batch produced:

| Path | Exact click lease | Session/origin diagnostic | Exact same-session relative | Result |
| --- | --- | --- | --- | --- |
| Add-on Trojan 1 | page `11`, control `91`, rectangle `320,200,175,30`, key `addon-trojan-01` | session `1`; independent origin `random-map/eligible` | `Map\Campaign\ao_trojan01.map` | `matched` |
| page-16 Roman 1 | page `16`, control `1903`, rectangle `237,148,175,30`, key `md-roman-01` | session `2`; independent origin `campaign/eligible` | `Map\Campaign\md_roman1.map` | `matched` |

Trojan proves that the independent presentation-origin classification no
longer suppresses a known exact descriptor lease. Roman proves the corrected
formatter family and exact relative. Across the complete session range there
were exactly two descriptor matches, zero descriptor rejections, and zero
`mcd2_` occurrences. Original and Dark Tribe were not repeated because their
unchanged anchors remained covered by static regression tests as designed.

## Normal-shutdown postflight

After the user normally closed the game and Settlers United:

- protected-process count: `0`;
- session log range: `[12894818,12970712)`;
- session log size: `75,894` bytes;
- session log SHA-256:
  `e37d920facc2bbf78fb09426ea5cd3dea777a7a18e98bec9e7eecd0b10b18dca`;
- final full log size: `12,970,712` bytes;
- final full log SHA-256:
  `97ae6bf6a56577fa707bece2150bd769d63ef93fab54388f44af7c9db689dc18`.

The installed archive, embedded ASI, and live INI retained their deployed
hashes. Database main and backup retained the exact preflight sizes,
timestamps, and hashes. Archive, INI, and database temporary siblings were all
absent.

Phase 6C.1 is therefore **GO** for the corrected immutable descriptor and
same-session association contract. This GO does not authorize campaign marker
indexing/rendering, database or save changes, or campaign-progress behavior;
those remain a separate Phase 6D design and approval boundary.
