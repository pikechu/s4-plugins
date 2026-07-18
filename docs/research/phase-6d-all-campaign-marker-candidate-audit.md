# Phase 6D all-campaign marker candidate audit

Date: 2026-07-18 (Asia/Shanghai)

## Decision

Implementation checkpoint
`2de918fe6e02196cdef2e6770a7838657861b017` is **STATIC GO** for a separately
approved guarded deployment of the Phase 6D all-campaign read-only marker
candidate. This audit is not deployment approval.

The candidate freezes all 107 `CLOSED` descriptors, restores the accepted
fixed-map marker path, and adds a separate campaign index and public snapshot
observer. The shared marker frame is exactly 36 commands, matching the maximum
simultaneously resident controls on composed page 6.

Completion detection, completion storage writes, native victory events, hooks,
patches, Lua writes, game-data writes, and campaign-progress writes remain
disabled.

## Read-only and identity boundary

- Startup calls the bounded file interface exactly once for the main
  `completed_maps.json`.
- Missing, unavailable, or malformed main data fails closed to an empty owned
  snapshot. The candidate does not read the backup, normalize, create,
  replace, or write any database file.
- Campaign index admission requires `Fixed`, `Campaign`, strict UTF-8, an
  exact self-consistent stable ID, and one exact admitted descriptor relative.
- Duplicate valid records make that descriptor ambiguous and suppress its
  marker.
- Display/save values are ignored. In particular, `RD_PlayerSave` does not
  change classification. Only the exact relative identifier is authoritative;
  case, separator, prefix, source, type, or stable-ID changes fail closed.
- Runtime descriptor/session validation continues to use confirmed
  same-MapInit-session `identity.relative`; it never uses `identity.name`.

## Local gates

- Full 98-source test target cross-compiled and linked as Win32 PE32: PASS.
- Focused compilation of all changed production translation units: PASS.
- `git diff --check`: PASS.
- Intermediate RED remained local and was squashed with GREEN before push.
- The untracked `docs/handoffs/` directory was excluded.

The local host does not provide CMake/MSVC or a Windows execution layer, so
the authoritative workflow supplies the MSVC build and executable test result.

## Authoritative Windows CI

- Workflow: `build-debug-asi`.
- Run: `29635655670`.
- Job: `88057416571`.
- Head SHA: `2de918fe6e02196cdef2e6770a7838657861b017`.
- Started: `2026-07-18T07:23:36Z`.
- Completed: `2026-07-18T07:26:17Z`.
- Result: PASS.

All steps passed: Settlers United archive integration, Win32 Release
configuration, MSVC build, marker/runtime policy, all forbidden-behavior
mutation fixtures, the complete test suite, packaging, PE32/export and package
verification, and artifact upload.

## Artifact audit

- Artifact ID: `8427025352`.
- Artifact name: `CampaignCompletionDebug-Win32`.
- GitHub-reported size: `264486` bytes.
- GitHub service digest and downloaded wrapper SHA-256:
  `d68b3b497f21e9b7cf3962e198339464c02b5ea8e0a8eab01c6b64be9f663c2e`.
- Candidate `CampaignCompletionDebug.zip` size: `264458` bytes.
- Candidate ZIP SHA-256:
  `99c3dceaa46057500b63b4c0853b63bbdd68ec45d5c36705ec4105b0e2c933eb`.
- Packaged ASI size: `484864` bytes.
- Packaged ASI SHA-256:
  `720566b1b0c8bab874a61b879f0f0c1a796562e7a4abede4437d79e5cc342d17`.
- Packaged INI size: `1355` bytes.
- Packaged INI SHA-256:
  `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`.
- Source INI size: `1311` bytes.
- Source INI SHA-256:
  `e7c356449183cc299bba3d328b097627fa6f032267c53618702ba6ee800e6cd7`.

The wrapper contains exactly one candidate ZIP. The candidate ZIP contains
exactly the project ASI and INI at their expected plugin paths. The packaged
INI is content-identical to the source after expected LF-to-CRLF packaging
normalization. The ASI is PE32/i386; the workflow independently verified its
export and package policy.

## Boundary and next action

No game or Settlers United process was started, stopped, or inspected. No
installed archive, plugin file, live database, backup, save, or campaign
progress was read or modified.

Deployment requires a fresh explicit approval naming this exact audited Phase
6D candidate after both protected applications have been normally closed.
Deployment must use the audited hashes above and the established guarded
rollback procedure. A later live acceptance may read the database only within
its separately approved bound and does not authorize a completion write or
victory transaction.

## Guarded deployment

The user normally closed both protected applications and explicitly approved
deployment of this exact audited Phase 6D candidate. Fresh preflight confirmed:

- protected-process count `0`;
- installed Phase 6D gap-only archive SHA-256
  `ea15116f8b84996c4c72e037d6b41782c4921f91224eec24c6acdc1d40e9ac67`;
- live `0.10.0` INI SHA-256
  `4a9835f5ea2228aea2780e2d393f001d87e09c35dd87f6525969cc69a0b01e38`;
- candidate ASI and INI exactly matched the audited hashes above;
- immutable original archive remained
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- database main/backup hashes and timestamps exactly matched the preceding
  normal-shutdown postflight;
- no authorized temporary sibling existed.

The fixed-hash elevated deployment script created and verified the complete
rollback snapshot at
`research/backups/campaign-completion/2026-07-18-pre-v0.11.0-phase6d-all-campaign-markers`.
The guarded installer replaced only the project ASI archive entry, and the
project INI was replaced atomically in its existing project-owned directory.

Independent post-deployment verification recorded:

| Item | Result |
| --- | --- |
| installed `Plugin_SU.zip` | `1,423,317` bytes; SHA-256 `80b16393c2cf5f719c2046ea18b3542606c634c38e2a4bc4c6ea018d6463ffb4` |
| embedded ASI | SHA-256 `720566b1b0c8bab874a61b879f0f0c1a796562e7a4abede4437d79e5cc342d17`, exact candidate |
| live INI | `1,355` bytes; SHA-256 `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`, exact candidate |
| database main | `1,260` bytes; timestamp `2026-07-16T05:41:41.3482563Z`; SHA-256 `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| database backup | `951` bytes; timestamp `2026-07-14T11:13:02.1756072Z`; SHA-256 `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

The installed archive contains all nine immutable original entries plus exactly
one project ASI and no missing original entry. Installer metadata now records
the deployed archive and embedded-ASI hashes. Protected-process count remained
zero; archive, INI, and database temporary siblings were absent. The
machine-readable result is retained at
`artifacts/phase6d-all-markers-2de918f/deployment-result.json`.

Deployment is complete. Dynamic marker acceptance remains pending. It does not
authorize a victory, database write, save access, campaign-progress mutation,
or process control.

## Fail-closed startup and evidence-hash correction

The first user-driven `0.11.0` startup loaded the exact deployed ASI and passed
the full executable compatibility gate. The bounded main-database read loaded
four records, while all six campaign descriptor groups correctly failed closed
and admitted zero campaign records. The database main/backup hashes and
timestamps remained unchanged and no temporary sibling appeared.

Diagnosis found no offset, length, executable, or file-reading defect. The 17
runtime constants had retained the correct frozen hash prefixes and suffixes
but contained non-authoritative middle text transcribed from an abbreviated
handoff. Direct file-window hashing matched every complete SHA-256 already
published in the frozen Phase 6A/6B/6D research documents.

Correction checkpoint
`ff9ccaf7780f95cb974f3fba33dc0ecd279be5eb` changes only those 17 constants
to the complete authoritative values and adds a policy regression enumerating
all of them. It does not change offsets, lengths, descriptor mappings, identity
rules, database behavior, renderer behavior, or any permission boundary.

Corrected authoritative CI:

- workflow run `29636512386`;
- job `88059697363`;
- started `2026-07-18T07:53:16Z`;
- completed `2026-07-18T07:55:40Z`;
- result PASS for Win32 build, policy and mutation gates, full tests,
  packaging, PE32/export verification, and artifact upload.

Corrected artifact audit:

- artifact ID `8427287968`;
- wrapper size `264571` bytes and SHA-256
  `4cc18349f481278c40aa2ed34287231325edbbe4260d1eb8162928756f6f4ca8`;
- candidate ZIP size `264493` bytes and SHA-256
  `16245b7cc282938b7082c3ba98ca3e7092d1d5ad232af4aaa5c7708f6d2813de`;
- corrected ASI size `485376` bytes and SHA-256
  `3898b7b28b8321fb2434ff946dd2280ee08bbb4f854663197ab99455bd79be41`;
- packaged INI remains the exact audited `1355`-byte file with SHA-256
  `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`.

The corrected artifact is **STATIC GO** but is not deployed. Replacement
requires the user to normally close both protected applications and explicitly
approve deployment of this exact Phase 6D evidence-hash correction candidate.

## Evidence-hash correction deployment

The user normally closed both protected applications and explicitly approved
the exact corrected candidate. Fresh preflight matched the fail-closed
deployment, candidate, immutable original, installer metadata, database
main/backup hashes and timestamps, and absence of all temporary siblings.

The fixed-hash elevated correction deployment created and verified a new
rollback snapshot at
`research/backups/campaign-completion/2026-07-18-pre-v0.11.0-phase6d-evidence-hash-fix`.
Because the packaged INI was already exact, the deployment replaced only the
project ASI archive entry and did not rewrite the INI.

Independent post-deployment verification recorded:

| Item | Result |
| --- | --- |
| installed `Plugin_SU.zip` | `1,423,333` bytes; SHA-256 `f584df8f64b6abb90945ae4617bb3bc43098b2eb597655524408c82b6217b836` |
| embedded corrected ASI | SHA-256 `3898b7b28b8321fb2434ff946dd2280ee08bbb4f854663197ab99455bd79be41`, exact corrected candidate |
| live INI | `1,355` bytes; timestamp `2026-07-12T16:00:00.0000000Z`; SHA-256 `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`; not rewritten |
| database main | `1,260` bytes; timestamp `2026-07-16T05:41:41.3482563Z`; SHA-256 `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| database backup | `951` bytes; timestamp `2026-07-14T11:13:02.1756072Z`; SHA-256 `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

Protected-process count remained zero and archive, INI, and database temporary
siblings were absent. The machine-readable result is retained at
`artifacts/phase6d-hash-fix-ff9ccaf/deployment-result.json`.

The corrected deployment is complete. Dynamic admission and marker acceptance
remain pending and retain all previously stated no-write and no-process-control
boundaries.

## Corrected live representative batch

The corrected user-driven startup loaded exact version `0.11.0`, passed exact
executable compatibility, and admitted all six descriptor groups:
Add-on, Mission CD, New World, New World 2, Original, and Dark Tribe. The
bounded main-database read loaded four records; all four were correctly
rejected from the separate Campaign index because none has Campaign source.
No database record was modified or manufactured for visual testing.

The user completed one no-launch, no-hover representative batch and returned
to the main menu. The accepted session contains 28 successful campaign
snapshots:

| Page/layout | Successful snapshots | Exact controls |
| --- | ---: | ---: |
| Add-on child page 11 | 3 | 12 |
| Mission CD child page 16 | 3 | 5 |
| Mission CD selector bonus page 4 | 2 | 1 |
| composed page 6, New World | 5 | 20 |
| composed page 6, New World 2 | 1 | 16 |
| Original page 20 | 5 | 9 |
| Dark Tribe page 21, including scroll/return | 9 | 12 |

There were zero invalid campaign snapshots, renderer failures, errors, MapInit
events, or database temporary files. Two transient fixed-map internal-menu
`alias-mismatch` warnings occurred while page 4 overlapped page transitions;
the read-only adapter failed closed as designed and neither warning affected
campaign capture or rendering.

The user also completed the fixed-map regression step in the same batch.
Campaign pages correctly displayed no false markers because the database
contains no Campaign completion record. Positive drawing for every one of the
107 descriptors remains covered by the authoritative all-record in-memory
test; no live completion record was created merely to force a marker.

## Normal-shutdown postflight

After the user normally closed both protected applications:

- protected-process count: `0`;
- final corrected session slice: `90,453` bytes; SHA-256
  `e514a182d12ca6f4be17a3b710a31da69e394d4b20ab6741d0aa8f913df05188`;
- final full log: `19,866,187` bytes; SHA-256
  `1eb96c620ffb1c2688f9f19ac6b616c28300079996b13db05052fc2525c61e7a`;
- installed archive remained `1,423,333` bytes with SHA-256
  `f584df8f64b6abb90945ae4617bb3bc43098b2eb597655524408c82b6217b836`;
- embedded ASI remained the corrected candidate
  `3898b7b28b8321fb2434ff946dd2280ee08bbb4f854663197ab99455bd79be41`;
- live INI remained
  `3e4bca3799f6a8ef5eb68dd8a42603b6ce241db5abe9145b22e2a57ef73a1b18`;
- database main remained `1,260` bytes with its original timestamp and
  SHA-256
  `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e`;
- database backup remained `951` bytes with its original timestamp and
  SHA-256
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`;
- archive, INI, and database temporary siblings were absent.

Phase 6D all-campaign immutable descriptors, separate read-only index, public
observer, shared 36-command renderer scheduling, fixed-map regression, guarded
deployment, and representative no-record integration are **GO**. This does not
record a campaign victory. Campaign completion admission and persistence
remain a separate future authorization boundary.
