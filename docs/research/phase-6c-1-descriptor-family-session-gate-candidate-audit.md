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
