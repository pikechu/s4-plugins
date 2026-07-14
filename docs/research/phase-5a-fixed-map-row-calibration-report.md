# Phase 5A Fixed-Map Row Calibration Report

## Result

The `0.5.0` fixed-map row-calibration candidate passed its Win32 build,
policy, test, packaging, and artifact checks at commit
`b314be045c1a16a1d4bafda78d886a153d290ba3`.

This is a calibration-only build. It observes candidate fixed-map rows and
writes one bounded project-owned trace below the existing
`Plugins\CampaignCompletion` data directory. It does not draw a completion
marker, create a UI control, add an internal hook, change completion admission,
or modify game data. `CompletionMarkers` remains `0`.

The candidate audit is **GO for guarded deployment and live Phase 5A
calibration**. The row-signature/rendering decision remains pending live
evidence; this report does not authorize Phase 5B rendering.

## Final CI evidence

- Workflow: `build-debug-asi`
- Run: `29309353803`
- Job: `87009603527`
- Head SHA: `b314be045c1a16a1d4bafda78d886a153d290ba3`
- Run URL: <https://github.com/pikechu/s4-campaign-tracker/actions/runs/29309353803>
- Conclusion: `success`
- Artifact name: `CampaignCompletionDebug-Win32`
- Artifact ID: `8301472389`
- Artifact API size: `617462` bytes
- Artifact API digest:
  `sha256:22f42e955bbd3e870ca33099895d6726298d40e0dde7875b211025ceeb47d4dd`
- CTest: `1/1` target passed in `0.87 s`; total time `0.89 s`

The workflow passed all of these gates against the final commit and artifact:

- Settlers United archive integration;
- Win32 `/W4 /WX` build;
- PE32 completion-persistence and zero-process-patch policy verification;
- rejection of forbidden process-patch, Lua-write, end-check, internal-hook,
  and related mutation fixtures;
- the complete CTest suite;
- package creation;
- PE32 machine and exact two-entry package verification; and
- artifact upload.

## Downloaded artifact inspection

The downloaded inner package and its extracted inspection copy are stored only
in this untracked project-owned directory:

```text
artifacts/phase-5a-row-calibration/run-29309353803/
```

| Item | Bytes | SHA-256 |
|---|---:|---|
| `CampaignCompletionDebug.zip` | 617781 | `d598056ba32934a675a83150fc75804b73808da269b6f8e4021d8a761f10d685` |
| `CampaignCompletionDebug.asi` | 1847808 | `c2177bc473dfdcb30b9f2f1d08c3257cbe858ec618e2844da2cddc2b47dc3326` |
| `CampaignCompletionDebug.ini` | 613 | `d90a2a0014769567abbcec4be8a2eab6a66289a1732578c3a828ed31f9a2a220` |

The inner package contains exactly:

```text
Plugins/CampaignCompletion/CampaignCompletionDebug.ini
Plugins/CampaignCompletionDebug.asi
```

Independent read-only inspection identified a six-section PE32 Intel 80386 DLL.
Reading the COFF machine field at PE offset `280` produced `0x014c`, matching
`IMAGE_FILE_MACHINE_I386`.

The temporary-archive Settlers United integration suite was also rerun locally
and passed. The host has no Visual Studio `dumpbin.exe`, so the full binary
policy script could not be rerun locally; its prerequisite failure was recorded
as `Win32 dumpbin.exe is unavailable`. The exact script ran successfully
against this final ASI in CI job `87009603527`, which reported:
`Verified PE32 completion persistence ASI, zero process patches, and read-only
Lua bridge.`

## Packaged policy

The packaged INI was extracted and read back with these material fields:

```ini
Version=0.5.0
CaptureTraceRoot=
DiagnosticMode=FixedMapRowCalibration
MarkerCalibration=1
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

The marker-calibration trace uses unique `CREATE_NEW` files named from the
current process ID, admits only the calibration record allowlist, and is capped
at `262144` bytes. No absolute game path, pointer, module base, arbitrary memory,
or unrelated UI text is admitted to that trace.

## Calibration implementation review

The final candidate builds an immutable fixed-map candidate index from the
successfully loaded completion-store snapshot. The index accepts only fixed,
offline records whose source/category pair and relative map directory agree;
random, campaign, online, malformed, and ambiguous identities fail closed.

The runtime forwards only public S4ModApi UI-frame, page-snapshot, approved-tab,
and GUI-element observations. GUI text is copied through a bounded SEH helper;
the coordinator receives value fields rather than retained game pointers. It
requires the exact fixed-map page set and known tab attribution, validates row
geometry against the reported surface, and records only candidate-name matches
plus their following frame boundary. Phase 5A contains no drawing API or marker
renderer.

Shutdown ordering was verified: calibration is disabled before public listener
removal, in-flight callbacks quiesce, the completion worker drains, and only
then are the calibration coordinator/index released and its trace closed.

During the GREEN run, the version bump exposed an important schema-1
compatibility defect: the decoder initially required every stored
`plugin_version` to equal the current build. That would have rejected the
existing valid `0.4.0` Aeneas and Antares records. Commit `a68182d` now admits
only persistence versions `0.4.0` and current `0.5.0`; legacy records remain
byte-preserved, new records identify `0.5.0`, and pre-persistence or unknown
versions remain rejected. The final CI suite covers this behavior.

No unresolved build, policy, persistence-compatibility, or package finding
remains. The GitHub Actions Node 20 deprecation annotation concerns the hosted
actions runtime and did not affect the plugin result.

## Deployment and live gate

After the user reported that the game and Settlers United were closed and
explicitly approved deployment, the process gate found neither `S4_Main` nor
Settlers United. Before any replacement, the current project-owned live data
directory was copied and verified file-by-file at:

```text
research/backups/campaign-completion/
  2026-07-14-pre-v0.5.0-row-calibration/Plugins/CampaignCompletion/
```

The backup contains four files and `1366583` bytes. Its INI SHA-256 is
`573a99ce24026b43901b0c4914b1b06ae6a6eb08f82826926695c88544ef5b2a`;
its database SHA-256 is
`b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`.

The first non-elevated guarded archive attempt was denied before it could
create the authorized temporary sibling. Post-failure verification found the
archive and metadata unchanged and no leaked sibling. The same existing
installer was then run elevated. Deployment and independent postflight values:

- immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- installed archive SHA-256:
  `2770385ae23e3caf6c8f6c786b4d8c9903fb979f1363ef56e3552fb40aa41745`;
- installed archive size: `1770734` bytes;
- embedded ASI SHA-256:
  `c2177bc473dfdcb30b9f2f1d08c3257cbe858ec618e2844da2cddc2b47dc3326`;
- installed INI SHA-256:
  `d90a2a0014769567abbcec4be8a2eab6a66289a1732578c3a828ed31f9a2a220`.

All seven protected original ZIP entries remained byte-identical. The archive
contains exactly those seven entries plus the one authorized
`Plugins/CampaignCompletionDebug.asi` entry. The INI was replaced through a
hash-verified same-directory temporary sibling. Both temporary siblings were
absent afterward, and the completion database retained its predeployment hash.

## Live calibration evidence

The user launched through Settlers United and performed each approved action
individually. The live module inventory identified the loaded ASI as the exact
audited candidate:

```text
PID: 31908
ASI SHA-256: c2177bc473dfdcb30b9f2f1d08c3257cbe858ec618e2844da2cddc2b47dc3326
Runtime: version=0.5.0 mode=fixed-map-row-calibration
Store: writable-loaded, records=2, failure=0, error=0
```

The final trace and copied log are stored only below the untracked evidence
directory `artifacts/phase-5a-row-calibration/live/pid-31908/`:

| Evidence | Bytes | SHA-256 |
|---|---:|---|
| `step-16-final.trace` | 14556 | `26dc584d8e558283bd30f0915616a5d55729b3d57ab3de02e5500a0014e82f44` |
| `CampaignCompletion-pid-31908.log` | 1449805 | `97a334a4ab17c72aa5fb3e7a72af462a312bb18ccfac6f7c1f57b2e60686e1a3` |

The copied files are byte-identical to their closed-process sources. The trace
contains 112 records: five state records, 54 unique candidate observations,
and 53 frame boundaries. One frame legitimately covers two consecutive
candidate callbacks; range validation proved that every candidate is covered
by a later frame record. There are zero malformed, ambiguous, wrong-category,
out-of-surface, out-of-order, or uncovered observations.

Observed state sequence:

```text
pages-fixed;list-unknown;generation-1
pages-fixed;list-single;generation-2
pages-fixed;list-custom;generation-3
pages-fixed;list-multiplayer;generation-4
pages-other;list-unknown;generation-5
```

Accepted public row signature and frame metadata:

- surface: `800 x 600`;
- fixed-list frame: page `4`, pillarbox `274`;
- row container: `0`;
- row x/width/height: `115 / 271 / 30`;
- observed row y slots: `142, 173, 204, 235, 266, 297`, a `31`-pixel pitch;
- state-dependent text style: `1` or `2`;
- slot-dependent value link: `2417` through `2422`;
- Aeneas: 18 `match-unique` observations, only under `list-single`;
- Antares: 36 `match-unique` observations, only under `list-custom`.

The value link is a visible-slot property, not map identity. During the Antares
scroll it changed with the row position (`2422` at y `297`, `2421` at y `266`,
`2419` at y `204`, `2418` at y `173`, and `2417` at y `142`). Rendering must
therefore use the current matched row rectangle and must never key completion
identity from `valueLink`.

Aeneas, the first map, disappeared under discrete scrolling and reproduced its
original `115,142,271,30` signature after returning and repainting. Antares
provided direct motion evidence: its rectangle moved through the visible slots,
stopped producing candidates when fully out of view, and returned at the new
current slots rather than reusing a stale rectangle. Normal, hover, and selected
states preserved identity and geometry; selection changed only the text style.

The Multiplayer Maps control recorded `list-multiplayer`; opening and scrolling
that list produced no Aeneas or Antares candidate. The final trace used only
`14556` of the `262144`-byte cap. The current-session log contains no warning,
error, timeout, overflow, ambiguity, listener, storage, or calibration failure.
The database SHA-256 remained
`b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`.
The user reported normal responsiveness throughout and a normal game/SU exit;
the final process gate found both processes closed.

## Phase 5A decision

**GO.** Every Phase 5A live criterion is satisfied. The evidence supports a
Phase 5B renderer that consumes the current exact candidate rectangle and draws
at the following public frame boundary, applies the pillarbox offset exactly
once, ignores `valueLink` as identity, and publishes completion-index updates
only after persistence commits.

The deployed Phase 5A configuration intentionally still has
`CompletionMarkers=0`. No marker-rendering behavior is enabled by this GO; a
separate reviewed and tested Phase 5B implementation remains required.
