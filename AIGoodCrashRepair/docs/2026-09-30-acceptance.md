# PileChainRepair 0.3.1 acceptance

Date: 2026-09-30 (Asia/Shanghai)

## Result and scope

**PASS for the known Pathes crashing save** on the admitted executable build.
The user confirmed a crash-free run, saving into `PileChainRepairz`, reloading
that save, and normal civilian transport. This acceptance covers that fixture
and the observed sessions; it does not establish coverage of every map or a
long-duration soak run.

Campaign Marker `0.13.4` remained installed with the same ASI hash and had
already passed the user's marker-placement check. Its
[Phase 7.4 audit](../../CampaignMarker/docs/research/phase-7-4-container-offset-marker-candidate-audit.md)
contains the build and marker acceptance record.

## Build and installation evidence

- Source commit: `421236b496c82e1acaeeac28f8a1b229b1a11dbc`.
- Authoritative [Windows CI](https://github.com/pikechu/s4-plugins/actions/runs/36718849781)
  passed the Win32 build, core and installer tests, PE/export checks, and package.
- The installed game ASI matched the SU archive entry.
- Every other current archive entry was preserved, including Campaign Marker.
- The verified legacy baseline remained restorable.
- The original crashing fixture and the separate acceptance copy stayed
  byte-identical through the run and save.

| Item | SHA-256 |
| --- | --- |
| `S4_Main.exe` (`2.50.1516.0`) | `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816` |
| `PileChainRepair.asi` (`0.3.1`) | `8d15897471516c162dac98376cd8760a7ea7bbf16d6305247a3ce353aba32cc4` |
| `CampaignCompletionDebug.asi` (`0.13.4`) | `de10824a451bec3d566dbf417307c23fb9912fbf6aa2ea2b03e98e9d20655534` |
| Installed `Plugin_SU.zip` at this checkpoint | `ea866285eb121209aed1c0d4c15d51a76a6b9b24b92a3936801b487de9cdb372` |
| Repaired `PileChainRepairz.sav` | `eed07103c8305e2d254788f334940aca4c2a650abef8c0c1cbba87db480b4595` |

Archive container hashes can change when a package is reconstructed; the
accepted ASI hashes identify the exact plugin bytes.

## Live run and repaired save

The test loaded the distinct `PileChainRepair-acceptance-20260930.sav` copy.
The original `QuickSave.crash-original.sav` was retained.

The captured runtime reported:

```text
2026-09-30T21:43:02.538+LOCAL [INFO] map initialized; validation armed for first game tick
2026-09-30T21:43:15.899+LOCAL [WARN] corruption detected tick=140065 kind=dangling-entity sector=2 good=8 entity=6302 expected-prev=4961 actual-prev=0
2026-09-30T21:43:15.899+LOCAL [INFO] local cut success=true
```

This session contains one successful cut, zero errors, and no full rebuild.
The loaded runtime identified itself as version `0.3.1`, `mode=local-cut`, and
reported executable compatibility as `compatible`.

About five minutes elapsed between the successful cut and the new save.
The user reported:
“没有崩溃 存储到PileChainRepairz了”. The new save was 1,857,404 bytes,
last modified at `2026-09-30 21:48:23` local time, with the hash listed above.

The next session initialized the reloaded save at
`2026-09-30 21:54:59.038` local time. The captured reload session contained
zero errors and no repeated corruption. Asked to check transport after
reloading, the user confirmed “都正常”.

## Evidence retained locally

The local, Git-ignored evidence directory is
`artifacts/pile-chain-repair-0.3.1/` at the workspace root:

- `verification.json` — build, installation, save hashes, and acceptance result.
- `acceptance-runtime.log` — captured initial and reload session logs.

The committed record intentionally includes no game executable, map binary,
user save, or proprietary archive backup.

## Installation and rollback

Package the accepted Campaign Marker `0.13.4` + PileChainRepair `0.3.1` pair with
`tools/package_accepted_release.ps1` from the workspace root. It defaults to
the verified artifacts and writes `Settlers4Plugins-0.13.4-0.3.1.zip` under
`dist/`. It accepts explicit `-CampaignAsiPath`, `-PileRepairAsiPath`, and
`-OutputDirectory` values.

Keep the complete extracted bundle together and install with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -GameDirectory 'F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4'
```

Use the actual game path. The SU path defaults to
`C:\Program Files\Settlers United`, or can be supplied through
`-SettlersUnitedDirectory`. The installer prints its transaction backup path,
by default under `%LOCALAPPDATA%\Settlers4Plugins\backups`. To restore the
state captured by that transaction:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rollback.ps1 -BackupDirectory '<backup directory printed by install>'
```

Close The Settlers IV and Settlers United before running the bundled
`install.ps1` or `rollback.ps1`. Installation verifies the accepted ASIs and
backs up the plugin versions immediately preceding that bundle installation.
Rollback uses that captured state. Retain the printed transaction backup directory
until the installed pair has been accepted again on the target machine.

For standalone PileChainRepair maintenance, the feature's
`tools/install.ps1` / `tools/uninstall.ps1` own only its SU archive entry. The
uninstaller restores the pre-install PileChainRepair entry, or removes it when
the installer originally added it. See the [feature README](../README.md).

## Repeatable live check

1. Preserve the original save and load a separate test copy.
2. Confirm `bootstrap version=0.3.1`, compatible executable, and
   `local cut success=true` in `Plugins/PileChainRepair/PileChainRepair.log`.
3. Run at normal speed for about five minutes and check transport.
4. Save to a new slot, reload it, and check transport for a further two to
   three minutes.
5. Record the new save hash and check the captured sessions for errors or
   repeated corruption.

A save that already contains the repaired chain should not require the same
cut again. The original crashing fixture remains the basis for reproducing the
initial cut.
