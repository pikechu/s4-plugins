# Phase 4 Completion Persistence Build Report

## Result

The `0.4.0` completion-persistence implementation passed its final Win32
build, policy, test, packaging, and artifact checks at commit
`b550375e9d321b981f35384929a781e31f8a44ef`.

The build-verification phase itself performed no deployment. After the user
subsequently approved guarded deployment, this exact verified `0.4.0` ASI and
INI were installed; the evidence and still-pending live gates are recorded
below.

## Final CI evidence

- Workflow: `build-debug-asi`
- Run: `29304084827`
- Job: `86993786685`
- Head SHA: `b550375e9d321b981f35384929a781e31f8a44ef`
- Run URL: <https://github.com/pikechu/s4-campaign-tracker/actions/runs/29304084827>
- Conclusion: `success`
- Artifact name: `CampaignCompletionDebug-Win32`
- Artifact ID: `8299610109`
- CTest: `1/1` target passed in `0.65 s`, containing 33 directly invoked test
  suites (32 calls plus the final runtime-policy suite)

The workflow also passed the Settlers United archive integration tests, the
PE32 and package-layout gate, the zero-process-patch/read-only-Lua verifier,
and every forbidden-behavior mutation fixture.

## Downloaded artifact inspection

The downloaded inner package is stored only in the project-owned inspection
directory:

```text
artifacts/phase-4-completion-persistence-ci/29304084827/
```

Hashes and sizes:

| Item | Bytes | SHA-256 |
|---|---:|---|
| `CampaignCompletionDebug.zip` | 602448 | `072b313d9e8726033cee09b046c9530a07f0099b88b715c8d5aa536b2da32cae` |
| `CampaignCompletionDebug.asi` | 1802752 | `b50fc1c9ae49ddeecedfa83496a1b59cc4ddcc6f47e9fb555af45502ec675096` |
| `CampaignCompletionDebug.ini` | 591 | `573a99ce24026b43901b0c4914b1b06ae6a6eb08f82826926695c88544ef5b2a` |

The package contains exactly:

```text
Plugins/CampaignCompletion/CampaignCompletionDebug.ini
Plugins/CampaignCompletionDebug.asi
```

Independent inspection identified the ASI as a six-section PE32 Intel 80386
DLL and found exactly the expected `CampaignCompletionDebugStop` export. The
project's temporary-archive Settlers United integration suite was rerun
locally and passed. The exact Visual Studio `dumpbin` verifier could not be
rerun locally because the host has no Win32 Visual Studio toolchain; the same
verifier ran successfully against this final build in job `86993786685`.

## Packaged policy

The packaged INI was read back from the downloaded ZIP and contains:

```ini
Version=0.4.0
CaptureTraceRoot=
DiagnosticMode=CompletionPersistence
NativeEventSubscription=1
NativeTerminalEventId=609
InternalVictoryHook=0
HookCount=0
CodePatchBytes=0
GameDefaultGameEndCheckCalls=0
LuaWrites=0
GameDataWrites=0
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=0
```

Completion detection and persistence are enabled. Completion-marker rendering
remains disabled for this phase.

## Review findings

The review covered the implementation range from the pre-design baseline
through `b550375`, including completion identity, JSON parsing, transaction
file operations, the worker, runtime lifecycle, listener integration,
configuration, packaging, and policy verification.

The review specifically checked:

- exact session matching and reset on MapInit;
- local `won` event `609` admission and rejection of loss/online sessions;
- direct/load stable-ID deduplication and recordable random-map data;
- strict UTF-8/schema/count/size validation;
- duplicate completion as a zero-write operation;
- stale temporary-file nonpromotion;
- corrupt-main/valid-backup read-only behavior with manual-action logging;
- complete temporary-file validation before atomic replace or first move;
- bounded queue admission and 5000 ms controlled drain;
- resource retention after drain timeout and process-lifetime runtime ownership;
- absence of new hooks, code patches, Lua writes, game-data writes, and
  `GameDefaultGameEndCheck` calls;
- module-relative writable paths below `Plugins\CampaignCompletion` and no
  persisted save/profile/absolute user paths.

One important issue was found and fixed during review: the stable-ID index was
previously rebuilt after the file commit. Commit `b550375` now prepares that
index before any file write and uses compile-time nothrow-move checks, so an
allocation failure cannot produce a new disk snapshot with stale in-memory
state.

No unresolved critical or important review findings remain. The CI annotation
about GitHub Actions Node 20 deprecation is external to the plugin build and
did not affect the result.

## Guarded deployment on 2026-07-14

After the user explicitly approved deployment, the first non-elevated archive
replacement attempt was rejected by Windows before the authorized temporary
sibling could be created. Post-failure hashes proved that the archive,
metadata, live INI, and immutable backup were unchanged and that no temporary
file remained. The user then ran the same guarded installer from an elevated
PowerShell session.

Before replacement, neither `S4_Main` nor Settlers United was running. The
migrated live configuration directory was copied and hash-verified at:

```text
research/backups/campaign-completion/
  2026-07-14-pre-v0.4.0-deployment/Plugins/CampaignCompletion/
```

Deployment evidence:

- installed `Plugin_SU.zip` SHA-256:
  `f712fd1c7485768e60688d0209ab4d50162cc544e604dc329eaf2107a17fd1be`;
- immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- embedded ASI SHA-256:
  `b50fc1c9ae49ddeecedfa83496a1b59cc4ddcc6f47e9fb555af45502ec675096`;
- installed INI SHA-256:
  `573a99ce24026b43901b0c4914b1b06ae6a6eb08f82826926695c88544ef5b2a`.

An independent read-only ZIP audit verified all seven original archive entries
byte-for-byte and found exactly one additional authorized entry,
`Plugins/CampaignCompletionDebug.asi`. The archive contains eight unique
entries after deployment. Backup metadata matches the installed archive and
embedded ASI. The temporary archive sibling is absent, the former root-level
`<game>\CampaignCompletion` directory is absent, and both relevant processes
remained closed throughout postflight verification.

Deployment is complete, but live game acceptance is still pending. It is not
yet a live GO.

## Original build-time deployment boundary

At build-report creation time, the report and downloaded artifacts were
project files only and no deployment had occurred. The later approved guarded
deployment is recorded above; no file outside its authorized archive, ASI, and
configuration scope was changed.

## Canonical map-category follow-up build

The direct/load consistency correction was completed at commit
`1ec15ecaebf49369ef392874f3b7b37886ac50e4`. Navigation and load controls
continue to decide admission, while validated SU relative map identity now
selects the canonical persisted category. Writable schema-version-1 records
are normalized through the existing verified atomic replacement transaction;
normalization changes only `launch_source`. A failed normalization retains the
original valid snapshot read-only, disables future writes, and logs that
manual handling is required.

TDD evidence:

- RED run `29305605022` built successfully and passed both policy checks, then
  failed CTest only at `a confirmed custom-map load becomes eligible and
  canonical`.
- GREEN classifier run `29305722340` passed the full workflow.
- RED store run `29305851426` failed Build only because the tests referenced
  the not-yet-defined `StrictUtf8ToWide` and
  `ReadOnlyNormalizationFailed` interfaces.
- GREEN store run `29305997273` passed the full workflow.

Final workflow evidence:

- Workflow: `build-debug-asi`
- Run: `29306106271`
- Job: `86999628073`
- Head SHA: `1ec15ecaebf49369ef392874f3b7b37886ac50e4`
- Run URL: <https://github.com/pikechu/s4-campaign-tracker/actions/runs/29306106271>
- Conclusion: `success`
- Artifact: `CampaignCompletionDebug-Win32`, ID `8300294794`
- Uploaded artifact digest:
  `95d3621e84184473841a35c3ac4942d91464ace54cb2aa9a6c81a0a4ac1ca311`
- CTest: `1/1` passed in `0.53 s`; total test time `0.56 s`

The run passed Settlers United archive integration, Win32 `/W4 /WX` Build,
the native-event policy verifier, every forbidden-behavior mutation fixture,
CTest, packaging, PE32/package verification, and artifact upload.

The downloaded candidate is isolated below the project-owned directory
`artifacts/run-29306106271/`. Independent inspection produced:

| Item | Bytes | SHA-256 |
|---|---:|---|
| `CampaignCompletionDebug.zip` | 604069 | `1e5c98e181bde279258fbf2da892a2218a2cbf35d613b71f5596e98260d91724` |
| `CampaignCompletionDebug.asi` | 1806336 | `3b403e94e26435f8cce36c52098b4787a3e66a137f1a61c0adc4a5a02ffbe502` |
| `CampaignCompletionDebug.ini` | 591 | `573a99ce24026b43901b0c4914b1b06ae6a6eb08f82826926695c88544ef5b2a` |

The ZIP contains exactly
`Plugins/CampaignCompletionDebug.asi` and
`Plugins/CampaignCompletion/CampaignCompletionDebug.ini`. The INI is
byte-identical to the repository configuration. Independent binary inspection
identifies the ASI as a six-section PE32 Intel 80386 DLL and finds the expected
`CampaignCompletionDebugStop` export.

Candidate inspection and the initial follow-up report changed only
project-owned files. The later user-approved guarded deployment is recorded
below.

## Canonical map-category guarded deployment

After the user reported that the game was closed and explicitly approved
deployment, a fresh process gate found neither `S4_Main` nor Settlers United.
The hash-gated wrapper required all of these exact preconditions before calling
the existing `SettlersUnitedArtifact` installer:

- installed archive:
  `f712fd1c7485768e60688d0209ab4d50162cc544e604dc329eaf2107a17fd1be`;
- immutable original archive:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- candidate ASI:
  `3b403e94e26435f8cce36c52098b4787a3e66a137f1a61c0adc4a5a02ffbe502`;
- live INI:
  `573a99ce24026b43901b0c4914b1b06ae6a6eb08f82826926695c88544ef5b2a`;
  and
- pre-start database:
  `c06a8e920c314ccccdd5826218a1ab4e99e5ac0a4cde1143d06ac54a2c555764`.

The elevated installer returned:

- installed archive SHA-256:
  `5b1abe961e8e19331bd2781cc036a9549929af784831e2d2720d1230987dfa32`;
- installed archive size: `1757412` bytes; and
- embedded ASI SHA-256:
  `3b403e94e26435f8cce36c52098b4787a3e66a137f1a61c0adc4a5a02ffbe502`.

An independent read-only postflight audit verified that all seven protected
original entries remained byte-identical, the archive contains exactly eight
unique entries, the only authorized CampaignCompletion entry contains the
candidate ASI, backup metadata matches all three archive hashes, and no
authorized temporary sibling remains. The live INI stayed byte-identical. The
database also retained its pre-start hash, proving that archive deployment did
not rewrite completion data. A final process gate remained clear.

The archive deployment is complete. Live startup and automatic normalization
of the existing Antares record remain pending; that acceptance must verify that
only `launch_source` changes to `single-player-custom-map` while the stable ID,
record count, and original completion timestamp remain unchanged.
