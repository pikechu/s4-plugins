# The Settlers IV plugins workspace

This repository is organized as a multi-plugin workspace for The Settlers IV:
History Edition with Settlers United.

## Accepted versions — 2026-09-30

| Plugin | Version | Acceptance result |
| --- | --- | --- |
| Campaign Marker (`CampaignCompletionDebug.asi`) | `0.13.4` | User confirmed campaign marker placement is normal. |
| AI Good Crash Repair (`PileChainRepair.asi`) | `0.3.1` | The known Pathes crash fixture was repaired, saved, reloaded, and transported goods normally. |

The accepted PileChainRepair build is source commit
`421236b496c82e1acaeeac28f8a1b229b1a11dbc`. Its
[Windows CI run](https://github.com/pikechu/s4-plugins/actions/runs/36718849781)
passed before deployment. The [acceptance record](AIGoodCrashRepair/docs/2026-09-30-acceptance.md)
records the ASI hashes, runtime result, and save verification. Campaign Marker
has its own [candidate audit and live acceptance](CampaignMarker/docs/research/phase-7-4-container-offset-marker-candidate-audit.md).

These results cover the exercised campaign marker UI and the known crashing
save on `S4_Main.exe` version `2.50.1516.0`. Broader maps and longer play sessions
remain outside this acceptance record.

## Install and rollback

Create `Settlers4Plugins-0.13.4-0.3.1.zip` with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\package_accepted_release.ps1
```

The packager defaults to the accepted artifacts under `artifacts/` and writes
to `dist/`; `-CampaignAsiPath`, `-PileRepairAsiPath`, and `-OutputDirectory`
allow explicit paths. The bundle includes both accepted ASIs, a hash manifest,
acceptance documents, and its own `install.ps1` / `rollback.ps1` entry points. Extract the complete
bundle and close The Settlers IV and Settlers United before either operation.
From the extracted bundle, install with the actual game directory:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -GameDirectory 'F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4'
```

The default SU directory is `C:\Program Files\Settlers United`; supply
`-SettlersUnitedDirectory` when it differs. The installer prints the backup
directory, which defaults to a new transaction directory under
`%LOCALAPPDATA%\Settlers4Plugins\backups`. Keep that directory, then roll back
with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rollback.ps1 -BackupDirectory '<backup directory printed by install>'
```

The bundle includes a configuration example and preserves existing INI files
and the completion database.

The bundle installer updates the two owned plugin entries in Settlers United's
`resources/bin/s4_artifacts/Plugin_SU.zip` and the selected game's plugin
copies. Rollback restores the versions captured immediately before that bundle
installation. Keep the original crashing save; load a separate acceptance copy
and save a repaired game into a new slot.

## Workspace layout

- `CampaignMarker/` — campaign and fixed-map completion tracking, in-game
  markers, classified completion manager, tests, configuration, and research.
- `AIGoodCrashRepair/` — narrowly admitted pile-chain repair, tests, installer,
  and acceptance record.
- `MusicDuplicatedRepair/` — diagnosis in progress; no accepted repair release.
- `third_party/` — shared SDK and import libraries.
- `tools/` — combined release packaging and installation tools.

Campaign Marker development and CI use `CampaignMarker/` as the project source
root. The root `CMakeLists.txt` also exposes the Win32 `PileChainRepair` target
and its tests.

## Next work

The accepted pair has a documented installation and rollback path. The next
feature is the duplicate music investigation: establish its build and collect
a reproducible runtime trace before selecting a repair.
