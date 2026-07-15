# Phase 5B fixed-map completion marker report

Date: 2026-07-15 (Asia/Shanghai)

## Status

Static implementation, code review, Release CI, artifact audit, and guarded
deployment are complete. Live navigation, rendering, shutdown, and
same-process refresh acceptance remain pending and no live GO/NO-GO has been
assigned.

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

Deployment is complete and independently verified. This is not yet a live GO.

Live GO additionally requires the planned navigation/hover/selection/scrolling
checks, the zero-marker control, normal input and shutdown behavior, and a
same-process newly committed victory whose marker appears without restart.
Until all those checks pass, the result remains **PENDING LIVE ACCEPTANCE**.
