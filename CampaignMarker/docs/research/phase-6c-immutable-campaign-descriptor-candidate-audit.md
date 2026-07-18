# Phase 6C immutable campaign descriptor candidate audit

Date: 2026-07-16 (Asia/Shanghai)

## Decision

Candidate `0.9.0` is **GREEN for deployment review only**. It is not deployed.
Replacing the installed Settlers United archive still requires a fresh exact
approval after the user has closed both protected processes.

This candidate adds only exact-version-gated immutable descriptor admission
and diagnostic comparison against the existing confirmed same-session
`identity.relative`. Completion storage, database access, save access, marker
rendering, internal menu access, native events, hooks, patches, writes, and
synthetic input remain disabled.

## Source checkpoint

- commit: `e7890c2f277ae6f35bdb11bc5b761586bdb51e6b`;
- commit subject: `feat: add immutable campaign descriptor diagnostics`;
- static evidence:
  `docs/research/phase-6c-immutable-campaign-descriptor-evidence.md`;
- untracked `docs/handoffs/` was not staged or committed.

The catalog contains only the observed Add-on, Mission CD, Original, and Dark
Tribe records listed in the evidence report. New World and New World 2 are
unconditionally disabled evidence gaps. Exact rectangle and exact relative
identifier mismatches fail closed; no presentation-name fallback exists.

## Local gates

- `git diff --check`: passed before commit;
- `tools/verify_no_process_patch.ps1 -SourceOnly`: passed;
- frozen EXE/PDB hashes and every selected RVA window were independently
  re-read from disk and matched the evidence report;
- ASLR review removed relocated absolute operands from runtime window matches.

The local host does not have `dumpbin.exe`, so the downloaded-binary invocation
of the policy script stopped at its explicit dumpbin availability gate. The
same full script, including dumpbin checks, passed on the authoritative Windows
CI runner. Local `file` and GNU `objdump` independently confirmed PE32 i386,
DLL characteristics, and the sole named export
`CampaignCompletionDebugStop`.

## Authoritative Windows CI

- workflow run: `29488225956`;
- job: `87587748369`;
- head SHA: `e7890c2f277ae6f35bdb11bc5b761586bdb51e6b`;
- conclusion: success;
- duration: 2 minutes 33 seconds.

The run passed:

- Settlers United archive integration fixture;
- Win32 Release configuration and build;
- zero-process-patch/read-only policy verification;
- all forbidden-behavior mutation fixtures;
- the full unit and runtime-policy test suite;
- package generation;
- PE32, export, and package-content verification;
- artifact upload.

The Node.js action-runtime deprecation annotation is infrastructure-only and
did not affect the successful build or artifact.

## Artifact audit

- artifact ID: `8371340487`;
- artifact name: `CampaignCompletionDebug-Win32`;
- service artifact size: 239,521 bytes;
- service digest:
  `sha256:3adc3ac44484f44aafcd52c9449c7a3a2964f53beee68ba9d1ee96e82870ed5c`;
- downloaded candidate archive `CampaignCompletionDebug.zip`: 239,476 bytes,
  SHA-256
  `7321b8861a47dfa80fc5fabe5051c72d3b3a13e992a52457001fc6ec4f391865`.

The candidate archive contains exactly:

| Path | Size | SHA-256 |
| --- | ---: | --- |
| `Plugins/CampaignCompletionDebug.asi` | 428,032 | `2079ca5d510fca250e4c2427b464e8dae7ab4c5f64521c4c429e3041de44af4e` |
| `Plugins/CampaignCompletion/CampaignCompletionDebug.ini` | 1,148 | `4e870e63d8cfeadfd38e90dedf26481acb34b5406a22f119017bcfa7a1b16850` |

The packaged INI is text-identical to the repository policy after the expected
LF-to-CRLF packaging conversion. The ASI is PE32 i386 and exports only the
controlled-stop entry point.

## Live acceptance boundary

If separately approved for deployment, one process and one uninterrupted batch
are sufficient:

1. Add-on Trojan control `91`, expect exact
   `Map\Campaign\ao_trojan01.map`;
2. Mission CD Roman control `1903`, expect exact
   `Map\Campaign\mcd2_roman1.map`;
3. Original Viking control `2039`, expect exact
   `Map\Campaign\viking02.map`.

Dark Tribe control `2083` retains its accepted unchanged anchor and does not
need another launch. An unavailable or locked representative leaves only that
group dynamically unanchored. It never authorizes database, save, or progress
changes. Postflight must preserve the completion database main and backup bytes
and timestamps and leave zero temporary siblings.

## Guarded deployment

The user confirmed both protected applications were closed and explicitly
approved deployment of the exact Phase 6C candidate. Preflight independently
confirmed zero processes and the accepted Phase 6B installed state:

- prior archive: 1,395,795 bytes, SHA-256
  `3faaa2c537713e04b642d4adb15a495650ee0f5479c5558c939abe009dfcf936`;
- prior embedded/live ASI SHA-256
  `097aac07991ed0f6324bfadec333afff8f16fe1ebd8a10cc6c72d660979666b1`;
- prior live INI SHA-256
  `55c664baaf4f58d38c970686e36859eae6b58b718cdce1eca0b442a5ae45a1c2`;
- database and backup identities exactly matched the Phase 6B postflight;
- archive, INI, and database temporary siblings were absent.

Because the SU archive is administrator-owned, deployment used a UAC-elevated
fixed-hash script. Before mutation it made and verified a complete rollback
snapshot at
`research/backups/campaign-completion/2026-07-16-pre-v0.9.0-descriptor-diagnostic`.
The established guarded installer replaced only the project ASI entry, and the
matching game-side INI used an atomic same-directory replacement.

Independent post-deployment verification produced:

| Item | Result |
| --- | --- |
| installed `Plugin_SU.zip` | 1,398,739 bytes; SHA-256 `058c4dc251b321d06f9a006af54a6b50b20857f010127cf8749a6fff96ef9a46` |
| embedded project ASI | SHA-256 `2079ca5d510fca250e4c2427b464e8dae7ab4c5f64521c4c429e3041de44af4e` |
| live project INI | 1,148 bytes; SHA-256 `4e870e63d8cfeadfd38e90dedf26481acb34b5406a22f119017bcfa7a1b16850` |
| immutable original archive | SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265` |

The archive retains ten ZIP entries including two directory entries. Comparing
its eight file entries against the immutable original found exactly one changed
entry: `Plugins/CampaignCompletionDebug.asi`.

Database state remained byte- and timestamp-identical:

- `completed_maps.json`: 1,260 bytes, timestamp
  `2026-07-16T05:41:41.3482563Z`, SHA-256
  `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e`;
- `completed_maps.json.bak`: 951 bytes, timestamp
  `2026-07-14T11:13:02.1756072Z`, SHA-256
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`.

Post-deployment protected-process count was zero and no authorized temporary
sibling remained. The exact machine-readable result is retained in the ignored
artifact directory as `artifacts/phase-6c-e7890c2/deployment-result.json`.
Deployment is complete; live anchor acceptance remains pending.

## Live anchor result: NO-GO

The deployed `0.9.0` process loaded the exact audited ASI and admitted the exact
executable. Runtime admission reported Add-on, Mission CD, Original, and Dark
Tribe admitted, with New World and New World 2 disabled. The user then completed
the three anchors in one uninterrupted process and returned to the main menu.

The authoritative results were:

| Intended anchor | Public click | Confirmed same-session relative | Descriptor result |
| --- | --- | --- | --- |
| Add-on Trojan 1 | page 11, control `91`, `320,200,175,30` | session 1, `Map\Campaign\ao_trojan01.map` | no association; pending click was not session-armed |
| Mission CD Roman 1 | page 16, control `1903`, `237,148,175,30` | session 2, `Map\Campaign\md_roman1.map` | `relative-mismatch`; expected `mcd2_roman1` |
| Original Viking 2 | page 20, control `2039`, `420,150,175,30` | session 3, `Map\Campaign\viking02.map` | `matched` |

The Mission CD mismatch is a direct rejection of the offline prediction. The
confirmed same-session relative is authoritative; menu labels, the S4ModApi
page enum name, and the earlier path-family interpretation cannot override it.
The affected group is NO-GO until its construction/dispatch/formatter chain is
re-audited.

The Add-on launch confirmed the predicted relative in session 1, but strict
acceptance also requires the public click-to-session association record. That
record was absent, so the anchor is not accepted by temporal proximity. The
click occurred about ten seconds before MapInit and remained within its own
30-second lease. The independent `LaunchOrigin` prerequisite nevertheless
failed to arm it. Phase 6C.1 must bind only an already-known exact descriptor
control and fail closed at the later exact relative comparison; it must not
infer an association after the fact.

Original Viking 2 is GO for its immutable descriptor and dynamic anchor. Dark
Tribe retains its previously accepted unchanged anchor. No marker authorization
results from either success.

## Normal-shutdown postflight

After the user normally closed the game and SU:

- protected-process count: zero;
- session log range: `[12833244,12894818)`;
- session log size: 61,574 bytes;
- session log SHA-256:
  `9379de5bd93aedc34a404179466f98340cc7f510df220f85dc583071cebbbdd2`;
- final full log size: 12,894,818 bytes;
- final full log SHA-256:
  `4be2f800b7a33dab9f32773399ca705349a4ac4ae7812660a68f8f6ec0d73a75`.

The installed archive, synchronized ASI, and INI retained their deployed
hashes. `completed_maps.json` and its backup retained the exact preflight sizes,
timestamps, and hashes. Archive, INI, and database temporary siblings were all
absent. Phase 6C remains NO-GO overall pending a separately approved Phase
6C.1 diagnostic correction.
