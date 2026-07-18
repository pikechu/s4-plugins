# Phase 2.1 Diagnostic Hardening Report

Date: 2026-07-13  
Target: The Settlers IV History Edition through Settlers United  
Plugin: `CampaignCompletionDebug.asi` version `0.2.1`  
Source branch: `main`

## Result

Phase 2.1 resolves the three diagnostic gaps carried from Phase 2. The ASI now
survives ordinary Settlers United synchronization without a watcher, reports
the exact page associated with each public UI callback registration, and can
quiesce callbacks and remove all public listeners under live-game control.

The build remains diagnostic-only. It installs no internal hook, does not detect
victory, does not persist completion state, and does not render a completion
marker.

Decision: **GO for planning the next read-only internal-probe increment.** This
decision does not authorize internal hooks or completion behavior; those require
a separate reviewed design and implementation cycle.

## Red-Green and CI Evidence

Each production increment followed a pushed red test and a Windows CI green:

| Increment | Red run | Green run |
| --- | --- | --- |
| Settlers United archive integration | `29225809303` | `29225947676` |
| Exact page observation | `29226012206` | `29226111821` |
| Reverse removal and stop request | `29226223860` | `29226325400` |
| Callback quiescence and control loop | `29226409070` | `29226521100` |

Final run `29226607387` at commit `785439b` passed the PowerShell archive
tests, Win32 configure/build, CTest, packaging, PE verification, exact package
layout check, and artifact upload.

- Artifact ID: `8269973590`
- Artifact name: `CampaignCompletionDebug-Win32`
- Artifact digest:
  `3ab152d4001992b2605896f69a58817bdb1c4a475d5f24c0847e5a2b69d0d0b1`
- Downloaded outer ZIP SHA-256: same as the artifact digest
- Inner deployment ZIP SHA-256:
  `ed3346cb2af656646fc5999a6ba749c77dec1d27992721d0e39fe3a98d332ed9`
- ASI SHA-256:
  `6a24fe6a7992fb13ef7f69a254ec24660f166a53818a901829e2c127250a5375`
- INI SHA-256:
  `1401e428773204db66b5a777c474cfcf3c5b1e0d4feeb3f29e90d652f4b45ac3`
- PE machine: `0x014c` (`IMAGE_FILE_MACHINE_I386`)
- ASI size: `1,987,584` bytes

Independent extraction found exactly:

- `Plugins/CampaignCompletionDebug.asi`
- `CampaignCompletion/CampaignCompletionDebug.ini`

## Settlers United Archive Integration

The only modified Settlers United input was the explicitly authorized archive:

`C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`

The first non-elevated installation attempt was denied by the Windows ACL while
creating `Plugin_SU.zip.campaigncompletion.tmp`. It did not replace the installed
archive: its SHA-256 remained the original value, the backup matched it, and the
temporary sibling was absent. Re-running the same guarded wrapper from an
elevated PowerShell completed the installation.

Recorded archive state:

- Original SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`
- Original size: `1,176,944` bytes
- Patched SHA-256:
  `7a38a5354098d3f4cf10c7631c4c324fa7cbbc400c95a1f9f9ba5cc211e9e65e`
- Patched size: `1,683,000` bytes
- Embedded ASI SHA-256:
  `6a24fe6a7992fb13ef7f69a254ec24660f166a53818a901829e2c127250a5375`

Independent ZIP inspection found seven original file entries and eight patched
entries. Every original entry hash was unchanged. The only added entry was
`Plugins/CampaignCompletionDebug.asi`. Metadata, installed archive, original
backup, and embedded ASI hashes all match. No temporary sibling remains.

The original archive and metadata are under the Git-ignored
`research/backups/settlers-united/` directory. Restore readiness passed without
performing a restore. The patched archive remains installed as specified by the
approved acceptance design.

## Persistent Normal Launch

Settlers United was launched normally with no restoration watcher. It created a
short-lived `S4_Main.exe` PID `34404`, followed by the formal responsive process
PID `532`. Settlers United synchronization placed the diagnostic ASI in the game
`Plugins` directory with the exact CI hash.

The formal process log recorded:

- `CampaignCompletionDebug bootstrap version=0.2.1 pid=532 hook-mode=public-only`;
- the diagnostic ASI with the exact CI SHA-256;
- `S4_Main.exe` version `2.50.1516.0` and the approved executable hash;
- `S4ModApi.dll` version `2.3.2.0`;
- `executable compatibility=compatible`;
- `diagnostic runtime started; no internal hooks installed`;
- no warning or error in the current bootstrap session.

A 64-bit PowerShell process could not directly enumerate the 32-bit ASI module,
but the plugin's in-process inventory recorded its own loaded module path, base,
size, and exact hash. This is the stronger observation for this test.

## Exact Page Attribution

The user visited Single Player Maps, Multiplayer Maps, and Custom Maps for at
least three seconds each and returned to the main menu. The log transitioned:

- `active=1 primary=1` at the main menu;
- `active=1,7 primary=7` entering Single Player;
- `active=4,7,22,23,25 primary=25` during the transition;
- `active=4,22,23,25 primary=25` while the map-list UI was stable;
- back through `1,7` to page `1`.

This proves that the dedicated callback thunks preserve child values `22`, `23`,
and `25`; the former ascending `IsCurrentlyOnScreen` parent-page masking is gone.
The public API reports all three map-list child pages active at the same time
while the user changes list tabs. Therefore the active-page set alone does not
identify the selected tab; representative mouse elements `2450`, `2451`, and
`2415` distinguish the three user actions. A later read-only probe must account
for this public-API limitation rather than treating `primary=25` as selected-tab
identity.

Representative GUI evidence included page `7`, element/value link `2404`,
rectangle `450,360,200,125`, and page `25` records around rectangles
`647,198,60,40`, `647,311,60,40`, and `512,516,43,65`.

## Controlled Stop and Post-Stop Stability

Only
`CampaignCompletion/CampaignCompletionDebug.stop` was created. The runtime
consumed and deleted it, then logged exactly one sequence:

```text
controlled stop requested
listeners stopped registered=75 removed=75 failures=0
diagnostic runtime stopped
```

The final stop line was line `1926`, and the log length was `143,454` bytes. The
user then navigated main menu to Single Player, waited at least five seconds,
returned to the main menu, and waited again. The log remained at line `1926`
with zero later `ui-pages`, `mouse`, `gui-elements`, or `map-init` records.
`S4_Main.exe` PID `532` remained present and responsive.

This verifies callback rejection, in-flight callback draining, complete reverse
listener removal, API release, inert post-stop behavior, and live-game
stability.

## Protected Inputs and Restore Readiness

`sha256sum -c research/evidence/manifest.sha256` reported `OK` for all 12
protected inputs:

- the three baseline Settlers United binaries;
- five baseline game ASI files;
- `S4ModApi.dll`;
- `S4_Main.exe`;
- `S4_MainR.pdb`;
- `ddraw.dll`.

The separately authorized `Plugin_SU.zip` matched its recorded patched hash, its
backup matched the recorded original hash, the embedded ASI matched the CI ASI,
and the non-mutating restore-readiness check returned true.

## Review Findings and Remaining Risks

Static review found no blocking issue in the guarded archive workflow, callback
gate, reverse-removal helper, stop-request helper, control loop, or per-page
callback construction.

Remaining constraints:

- installation under `Program Files` requires an elevated PowerShell process;
- public page callbacks expose the three map-list children concurrently and do
  not by themselves identify the selected list tab;
- restore safety is hash-verified but the live acceptance intentionally left the
  patched archive installed instead of exercising a restore/repatch cycle on the
  real proprietary archive;
- completion detection, map identity, storage, markers, and internal probes are
  still absent and unverified.

Within the approved Phase 2.1 scope, persistent deployment, child callback
attribution, controlled cleanup, post-stop silence, process stability, and
protected-input integrity all passed. Planning the next read-only probe is
therefore **GO**.
