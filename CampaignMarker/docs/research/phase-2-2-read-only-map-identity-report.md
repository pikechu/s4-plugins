# Phase 2.2 Read-Only Map Identity Report

## Decision

**PARTIAL/NO-GO**

The three fixed-map lists are attributed reproducibly through public S4ModApi
UI callbacks. A stable fixed-map identity before and after `map-init` is absent
because static analysis did not prove one persistent internal object chain.
Following the approved fail-closed gate, no internal reader, hook, write, call,
heap scan, victory detector, completion storage, or marker behavior was built.

This matches the approved decision rule: tabs attributed, stable identity
absent, safety and integrity checks passed.

## Static Evidence Gate

The reviewed evidence is in
`docs/research/phase-2-2-static-map-identity-evidence.md`. The collector was
hash-gated to `S4_Main.exe` version `2.50.1516.0`, SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
Two consecutive collections produced an empty normalized diff.

The menu list provides a bounded ANSI path candidate at entry offset `0x00`,
but those menu entries are later destroyed. The fixed-map loader constructs a
stack-local `CMapFile` and destroys it before returning. The persistent lobby
settings root begins with a wide string, but the reviewed instructions do not
prove it is the selected map path or connect it to a persistent post-init
`CMapFile`. The only global-rooted `CMapFile` found belongs to random maps and
is out of scope. The gate therefore concluded **REJECTED FOR INTERNAL READ-ONLY
PROBE**.

Tasks 2-4 and 6-8 were skipped as required. No rejected RVA or guessed offset
was transcribed into production code.

## TDD and CI Evidence

All C++ validation used the Windows Win32/x86 GitHub Actions workflow because
the local workspace has no MSVC toolchain.

| Purpose | Commit | Run | Expected/observed result |
| --- | --- | ---: | --- |
| Initial list-attribution RED | `cf196da` | `29230917476` | Failed at missing `identity/ListAttribution.h`; archive integration passed |
| Calibration capture GREEN | `7dae39c` | `29231069368` | Archive, Win32 build, CTest, package, PE32 check, artifact upload passed |
| Calibrated mapping RED | `b095cf9` | `29231608224` | Failed only because `kApprovedTabControls` was absent; archive integration passed |
| Calibrated mapping GREEN | `29d4e4c` | `29231757865` | Complete workflow passed |
| Stable list-name RED | `c420f45` | `29231825900` | Failed only because `FixedMapListKindName` was absent; archive integration passed |
| Production attribution GREEN | `c74568a` | `29231935058` | Complete workflow passed |

The final artifact and `origin/main` both identify commit
`c74568a4bcc5a4b90df4ec412c4726cde3548780`.

## Final Artifact

| Property | Value |
| --- | --- |
| Workflow run | `29231935058` |
| Artifact ID | `8271869008` |
| Artifact name | `CampaignCompletionDebug-Win32` |
| Artifact size | `527659` bytes |
| GitHub artifact digest | `sha256:b85927f8b0c481aabe93c44c49d3aec7bb472da7c50769ce84a36e847a09dd48` |
| Packaged `CampaignCompletionDebug.zip` SHA-256 | `938cb4345ad6fdd60fa3fc56f5bea775d432d2b7c8de0aa2545158ba5f49eb6f` |
| ASI SHA-256 | `34a18e1ed9b26aec57ff86187f28af88cb72c4560d848b2d30e939e3898851c9` |
| INI SHA-256 | `1401e428773204db66b5a777c474cfcf3c5b1e0d4feeb3f29e90d652f4b45ac3` |
| PE target | `pei-i386`, architecture `i386` (`0x014c`) |

The packaged ZIP contained exactly:

```text
CampaignCompletion/CampaignCompletionDebug.ini  445 bytes
Plugins/CampaignCompletionDebug.asi              1994752 bytes
```

The installed `Map` tree contained exactly `373` regular recursive `*.map`
files before final deployment, matching the approved baseline.

## Tab Calibration

The complete action-level calibration and negative controls are in
`docs/research/phase-2-2-tab-calibration.md`. The approved production mapping,
in `single`, `multiplayer`, `custom` order, is `{2449, 2450, 2451}`.

| List | Calibration record | Final acceptance record |
| --- | --- | --- |
| Single Player Maps | `15:13:56.201`, ID `2449`, pages `4,22,23,25` | `15:30:23.593` and `15:33:52.540`: `list_kind=single`, ID `2449` |
| Multiplayer Maps | `15:14:57.146`, ID `2450`, pages `4,22,23,25` | `15:31:56.656`: `list_kind=multiplayer`, ID `2450` |
| Custom Maps | `15:15:22.840`, ID `2451`, pages `4,22,23,25` | `15:33:03.138`: `list_kind=custom`, ID `2451` |

Final-build row clicks produced IDs `2422` (Single), `2419` (Multiplayer),
and `2423` (Custom), with no `fixed-map-list` record. The calibration Back
control produced ID `2415` and left the exact fixed-map page set. None collide
with an approved tab ID. The final build emitted no `tab-calibration` records.

## Map Start and Identity Result

The user returned to Single Player Maps, selected a fixed map, started it, and
remained in the initialized map without winning. Public callbacks recorded:

```text
2026-07-13T15:34:31.516+LOCAL [INFO] map-init observed
```

No menu or loaded map path/internal ID was sampled. There are therefore no
identity source, confidence, or session-ID records to compare. This is an
explicit absence caused by the static gate rejection, not a hidden retry or a
menu/loaded conflict. The implementation did not fall back to scanning or an
internal hook.

## Guarded Deployment

The final ASI was installed only after `S4_Main.exe` and all Settlers United
processes were absent. The user ran the guarded installer elevated because the
archive is beneath `Program Files`.

| Property | Value |
| --- | --- |
| Archive | `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip` |
| Recorded original SHA-256 | `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265` |
| Original size/time | `1176944` bytes; `2026-05-10T08:38:14Z` |
| Final patched SHA-256 | `d4a6d598d9b1617e002f014c0d9e009cb776f4f058a83b72b86af8cc53572fb9` |
| Final patched size/time | `1685222` bytes; `2026-07-13T07:29:24.1139926Z` |
| Embedded ASI SHA-256 | `34a18e1ed9b26aec57ff86187f28af88cb72c4560d848b2d30e939e3898851c9` |

An independent read-only reopen found 10 archive entries and reproduced the
embedded ASI hash.

## Controlled Stop and Integrity

While the initialized game remained responsive, only
`CampaignCompletion/CampaignCompletionDebug.stop` was created. The plugin
consumed the file and recorded:

```text
2026-07-13T15:37:19.896+LOCAL [INFO] controlled stop requested
2026-07-13T15:37:19.897+LOCAL [INFO] listeners stopped registered=75 removed=75 failures=0
2026-07-13T15:37:19.897+LOCAL [INFO] diagnostic runtime stopped
```

The log then remained exactly `3026` lines and `250063` bytes with mtime
`2026-07-13 15:37:19.897810300 +0800` after the user deliberately opened and
closed an in-game menu. `S4_Main.exe` remained present and responsive. This
proves post-stop callback silence for the tested navigation.

`sha256sum -c research/evidence/manifest.sha256` returned `OK` for all 12
protected inputs: three baseline Settlers United binaries, five baseline game
ASI files, `S4ModApi.dll`, `S4_Main.exe`, `S4_MainR.pdb`, and `ddraw.dll`.
Additionally:

- installed archive SHA-256 remained
  `d4a6d598d9b1617e002f014c0d9e009cb776f4f058a83b72b86af8cc53572fb9`;
- original backup SHA-256 remained
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- both installed archive and original backup passed complete ZIP tests;
- the installed embedded ASI matched the final CI ASI;
- the non-mutating Settlers United archive integration test passed.

Restore readiness is therefore **true**. No proprietary archive, ASI, runtime
log, raw memory dump, or backup metadata is included in this report commit.
