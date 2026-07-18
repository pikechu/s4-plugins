# Phase 5B fixed-map completion marker report

Date: 2026-07-15 (Asia/Shanghai)

## Status

Static implementation, code review, Release CI, artifact audit, and guarded
deployment are complete. Live acceptance found a contained DirectDraw marker
rendering failure. Phase 5B is **NO-GO**; no same-process refresh test was
attempted after the required initial-marker criterion failed.

## Scope and authorization boundary

- Development remained on `main` as requested.
- No intermediate Task 2-5 build was deployed.
- No game or Settlers United file was modified during development or artifact
  audit. Deployment occurred only after the user confirmed both protected
  processes were closed and explicitly approved the audited Phase 5B candidate.
- `artifacts/` and `research/evidence/save-samples/` remained untracked and
  were not staged, deleted, or rewritten.
- The guarded deployment modified only the installed project INI and the exact
  Settlers United `Plugin_SU.zip` replacement target. No process was started or
  terminated by the deployment.

## Implementation checkpoints

The Phase 5B implementation range reviewed was `df9e04f..b1e5c28`.

| Checkpoint | RED | GREEN | Windows CI result |
| --- | --- | --- | --- |
| Commit-only marker refresh | `0f4d892` | `303b4f8` | `29311385284`, success |
| Fixed-capacity row observer | `74db4f6` | `fb3992e` | `29389881555`, success |
| Checked marker geometry | `d89aab2` | `ac15bee` | `29390126142`, success |
| Failure-contained renderer | `3e73435` | `4640193` | `29390410396`, success |
| Runtime/Release integration | `9a26413` | `9fb655d` | `29391115422`, success |
| Synchronized shutdown review fix | `0b4d84e` | `b1e5c28` | `29391467302`, success |

Each GREEN run passed archive integration, Win32 `/W4 /WX` build, mutation
policy gates, all unit tests, package creation, PE32/package verification, and
artifact upload. Task 5 and later runs used the Release configuration.

## RD-prefixed save-name rule

The runtime classification boundary remains the confirmed session-bound
relative identity returned by Settlers United, never the player-chosen save or
display name.

The regression in `CompletionCandidateCoordinatorTests.cpp` uses:

- confirmed identity name: `RD_PlayerSave`;
- confirmed relative identity: `Map\User\Antares.map`;
- expected kind: `CompletionMapKind::Fixed`;
- expected source: `SinglePlayerCustomMap`;
- fixed-list marker match: unique for `Antares`.

The same suite preserves genuine random classification for confirmed relative
identifier `RD_LCGSDR30`; random victories remain recordable but excluded from
the fixed-list marker index.

## Code review

The review covered wrong-row and stale-frame risk, duplicate/ambiguous rows,
checked pillarbox geometry, DC and pen lifetime, exception containment,
shutdown ordering, commit-only publication, callback-path allocation/I/O, and
persistence behavior.

One actionable defect was accepted: the control thread disabled the observer
and renderer before `S4Listeners::Stop()` had waited for in-flight public
callbacks, while both marker objects held unsynchronized mutable state. The
deterministic RED test `0b4d84e` blocked a render backend and proved that
`Disable()` could finish concurrently. GREEN `b1e5c28` added object-level
mutexes around observer and renderer public state operations. `Disable()` now
waits for an in-flight render/observation without adding heap allocation to the
success path. Release CI `29391467302` passed the regression and all existing
gates.

No remaining accepted finding was identified:

- row admission requires the exact calibrated page set, list kind, surface,
  container, rectangle pattern, paired slot/value link, and text style;
- fixed arrays bound text, observations, commands, and geometry;
- identical callbacks deduplicate, same-label/different-rectangle observations
  suppress that label, and different labels in one slot invalidate the frame;
- page/tab changes, page-4 consumption, invalidation, and disable clear pending
  state;
- pillarbox is supplied by the current callback and applied exactly once with
  checked 64-bit arithmetic;
- a non-empty valid frame performs one Describe/Begin/End pass, and End runs
  after draw failure;
- GDI drawing and DC ownership remain isolated in
  `DirectDrawMarkerSurface.cpp`, with precreated pens and SaveDC/RestoreDC;
- rendering failure disables only marker rendering after three consecutive
  failed frames and does not affect completion persistence;
- `CompletionWorker` snapshots and publishes only after `Committed`; duplicate,
  read-only, and failed results never publish;
- a valid read-only backup snapshot seeds the initial marker index;
- shutdown drains and destroys worker, renderer, surface, observer, index, and
  store in dependency-safe order.

The plan-named `requesting-code-review` skill was unavailable in this session,
so the review was performed directly against the complete implementation range
with the repository tests and policy gates.

## Audited Release artifact

- Candidate commit: `b1e5c283b02d194b56c0aba0621ebbf2dce10a0a`
- Workflow: `build-debug-asi.yml`
- Run: `29391467302`
- Job: `87275726782`
- Run conclusion: success
- Artifact name: `CampaignCompletionDebug-Win32`
- Artifact ID: `8333440702`
- Artifact API size: 218,448 bytes
- Artifact API digest:
  `sha256:143c4e78df32010f22c58c52bc1896db42e250551d7332cfc6fb7df6d4870c16`
- Untracked audit directory:
  `artifacts/phase-5b-completion-markers/run-29391467302/`

The downloaded artifact contained the publishable inner archive:

- `CampaignCompletionDebug.zip`
- size: 218,460 bytes
- SHA-256:
  `418f5ce6d040e72c081fdf872c01491f0a74345d6728a9e3d25bc66b9eac881d`
- `unzip -t`: no errors

The inner ZIP contains exactly two files:

| Entry | Size | SHA-256 |
| --- | ---: | --- |
| `Plugins/CampaignCompletionDebug.asi` | 436,224 | `b5db2bf25b4c4ee0a4de195b686a8dedefbe5b5e26661362ad7e28cff83527e4` |
| `Plugins/CampaignCompletion/CampaignCompletionDebug.ini` | 616 | `4f8aa0962482b6bc4dd63f91bd5734d749dbe436656084733a176dd9afff6c83` |

The ASI PE header was independently read from the extracted candidate:

- PE offset: 280;
- machine: `0x014c` (`IMAGE_FILE_MACHINE_I386`);
- optional-header magic: `0x010b` (PE32).

The packaged INI was inspected and requires:

```text
Version=0.6.0
DiagnosticMode=CompletionMarkerRendering
MarkerCalibration=0
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=1
```

The exact two-entry layout was verified by CI and local extraction. The local
temporary Settlers United archive integration test also passed. Local
`dumpbin` remained unavailable; the successful CI policy verifier performed
the export, PE32, production-source membership, and forbidden-mutation audit.

## Guarded deployment

The user confirmed that `S4_Main` and Settlers United were closed and explicitly
approved deployment of the audited Phase 5B candidate. Preflight found zero
protected processes and verified the installed archive against its prior
metadata before any write:

- prior installed archive SHA-256:
  `2770385ae23e3caf6c8f6c786b4d8c9903fb979f1363ef56e3552fb40aa41745`;
- immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- prior live INI SHA-256:
  `d90a2a0014769567abbcec4be8a2eab6a66289a1732578c3a828ed31f9a2a220`;
- completion database SHA-256:
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`.

The complete eight-file live configuration was copied and verified
file-by-file before deployment at:

`research/backups/campaign-completion/2026-07-15-pre-v0.6.0-completion-markers/Plugins/CampaignCompletion/`

The established guarded Settlers United installer replaced only the project
ASI entry in `Plugin_SU.zip`; the project INI was installed through an atomic
same-directory replacement. Independent post-deployment verification found:

| Installed item | Result |
| --- | --- |
| `Plugin_SU.zip` | 1,377,638 bytes; SHA-256 `997e6146c70d7f3c0314dff99f864e3d3b1e9c251ab6509d95bd437f74fc473d` |
| embedded `Plugins/CampaignCompletionDebug.asi` | SHA-256 `b5db2bf25b4c4ee0a4de195b686a8dedefbe5b5e26661362ad7e28cff83527e4` |
| live `CampaignCompletionDebug.ini` | SHA-256 `4f8aa0962482b6bc4dd63f91bd5734d749dbe436656084733a176dd9afff6c83` |
| live `completed_maps.json` | unchanged; SHA-256 `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

The installed archive has ten ZIP entries including two directory entries.
The ASI is its only changed entry; all other nine entries are byte-identical to
the immutable original. The original archive remained unchanged, all seven
non-INI live configuration files matched the verified backup, both authorized
temporary siblings were absent, and the protected-process count remained zero.
Deployment evidence is retained untracked at
`artifacts/phase-5b-completion-markers/run-29391467302/deployment-result.json`.

## Deployment and live acceptance gates

Deployment is complete and independently verified, but live acceptance is
**NO-GO**.

The deployed process loaded the exact audited ASI and logged:

```text
CampaignCompletionDebug bootstrap version=0.6.0 pid=23540 mode=completion-marker-rendering
```

At startup, the pre-existing primary completion database failed strict decode
with `CompletionJsonFailure::InvalidRecord` (`failure=10`). The valid backup
loaded read-only with two records, `Aeneas` and `Antares`, and seeded the marker
index as designed. This read-only state would independently prevent the later
same-process commit test; neither database file was modified.

The user opened the fixed single-player list with `Aeneas` visible. The normal
row showed no marker. Clicking the approved single-player tab produced repeated
`fixed-map-list list_kind=single element-id=2449` evidence, but the marker still
did not appear. Hovering and moving away also produced no marker or residual
artifact. Live GUI evidence included the calibrated Aeneas row
`115,142,271,30`, value link `2417`, and the exact page set `4,22,23,25`.

The renderer then logged three contained failures:

```text
2026-07-15T14:22:57.555+LOCAL [ERROR] completion-marker-renderer frame-failed
2026-07-15T14:23:02.613+LOCAL [ERROR] completion-marker-renderer frame-failed
2026-07-15T14:23:02.953+LOCAL [ERROR] completion-marker-renderer disabled failures=3
```

This proves that matching reached a non-empty render attempt and localizes the
live defect to the DirectDraw surface/DC/geometry render path rather than RD
archive-name classification, the completion index, list attribution, or row
identity. Failure containment worked: only marker rendering was disabled, the
game remained responsive, and no persistence mutation followed.

The user exited the game and Settlers United normally. The final protected
process count was zero. Post-exit SHA-256 values remained:

- live INI: `4f8aa0962482b6bc4dd63f91bd5734d749dbe436656084733a176dd9afff6c83`;
- primary database: `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`;
- database backup: `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`;
- installed SU archive: `997e6146c70d7f3c0314dff99f864e3d3b1e9c251ab6509d95bd437f74fc473d`;
- immutable original archive: `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

No rollback has been performed. Rollback or deployment of a future diagnostic
or corrected candidate requires a separate explicit approval after its scope
and artifact are audited.

## Render-stage diagnostic follow-up

The generic live `frame-failed` record could not distinguish surface
description, checked geometry, `GetDC`, GDI drawing, or `ReleaseDC`. RED
`d9a9a0a` required stable bounded stage labels for `describe`, `geometry`,
`begin`, `draw`, and `end`, plus the final stage on terminal disable. Windows
CI run `29394445791` passed archive integration, build, and policy gates, then
failed only at the expected missing `stage=geometry` test assertion.

GREEN `5586f79` added failure-stage classification without changing the row
observer, geometry algorithm, DirectDraw operations, completion persistence,
or classification policy. It also moved failure-path string construction
inside the existing exception boundary. Release CI run `29394873528`, job
`87285983210`, passed all gates. Its artifact `8334709514` had API digest
`sha256:f77e408c9e59d89782dc9829ef4829fb8d0ed07f3947896f743b2d362982c5e3`.
The audited two-entry inner ZIP had SHA-256
`b5a1af1bf8a47e6aa783a6ab8f27640232d4f03fc6802edc1a1816a3164784af`;
the PE32/i386 diagnostic ASI had SHA-256
`0a78c6bac79e540705986cf3f7aa0d70731268a272d519ddddde55a998e29332`.
The packaged INI was byte-identical to the installed INI and was not deployed.

After separate explicit user approval and a zero-process preflight, the
established guarded installer replaced only the project ASI entry. Independent
verification found:

- installed archive: 1,378,045 bytes, SHA-256
  `a1b9cac9cbdccefa0683b758fc9777ca859930f8a21856ce65068329e26a15a5`;
- embedded ASI: SHA-256
  `0a78c6bac79e540705986cf3f7aa0d70731268a272d519ddddde55a998e29332`;
- all other nine ZIP entries byte-identical to the immutable original;
- immutable original, live INI, primary database, and database backup unchanged;
- no temporary archive sibling and a final protected-process count of zero.

This diagnostic deployment does not change the Phase 5B **NO-GO** decision. A
short live reproduction is still required to identify the exact failing stage;
any corrected candidate remains subject to a new audit and explicit deployment
approval.
