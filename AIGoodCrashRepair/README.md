# AI Good Crash Repair

## Accepted build

Version `0.3.1` passed live acceptance on 2026-09-30 for the known Pathes crash
fixture. The runtime cut the link from entity `4961` to missing entity `6302`,
the user ran and saved without a crash, then reloaded `PileChainRepairz` and
confirmed normal transport. The captured initial and reload sessions contain
no errors or repeated corruption. See the
[acceptance record](docs/2026-09-30-acceptance.md) for the build, hashes, and
scope.

For the accepted Campaign Marker `0.13.4` + PileChainRepair `0.3.1` pair, use
the bundle created by `../tools/package_accepted_release.ps1` and its bundled
`install.ps1` / `rollback.ps1`. The feature installer below is useful when
maintaining PileChainRepair independently.

## Implementation and maintenance

All implementation work for this feature lives below this directory:

- `src/` — plugin runtime and pile-chain repair core
- `tests/` — standalone core and archive-installer integration tests
- `tools/` — guarded Settlers United installer and entry-level uninstall
- `backups/` — installer-owned backups (created on first installation; ignored
  by Git)
- `repro/` — local crashing map/save fixture, trace, hashes, and instructions;
  proprietary fixture binaries are excluded from Git
- `docs/` — committed acceptance records

The repository root `CMakeLists.txt` exposes the `PileChainRepair` target and
its core and archive-installer tests; feature sources remain below this folder.

`PileChainRepair.asi` is a narrowly admitted runtime repair for the pile-list
corruption observed in `Trace079.txt` and `Trace080.txt`.

It runs only when `S4_Main.exe` is version `2.50.1516.0` with SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
Other executable builds are rejected without changing game memory.

On each game tick the plugin validates every economy-sector pile chain for
fatal traversal faults: missing entities, non-pile entities, inaccessible
memory, and cycles. When corruption is detected, it cuts only the single link
that points at the fatal node. It does not rebuild economy-sector lists or
reassign healthy piles.

The visited-entity workspace is reused between ticks. Every tick scans the
current sector pointer table, including economy sectors created during play.
Version `0.3.1` also serializes map/tick callbacks with the stop barrier so a
delayed callback cannot write game memory after controlled stop.

The runtime log is written beside the plugin at:

`Plugins\PileChainRepair\PileChainRepair.log`

To stop its listeners without ending the game, create:

`Plugins\PileChainRepair\PileChainRepair.stop`

Build the Win32 target from the repository root with the Visual Studio
generator and `-A Win32`. The CTest suite includes the core tests and a
PowerShell integration test that exercises install, update, legacy metadata
migration, and uninstall against temporary synthetic ZIP archives.

Install or update only while `S4_Main.exe` and Settlers United are closed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\AIGoodCrashRepair\tools\install.ps1 -AsiPath .\build\Release\PileChainRepair.asi
```

The installer stores the original `Plugins/PileChainRepair.asi` entry, when
one existed, under `backups/`. It validates the replacement by per-entry
hashes, so changes to Campaign Marker or another plugin do not block an
update. On failure it restores the exact archive present immediately before
that install attempt. It does not roll the whole archive back to the first
install version.

Remove this plugin and restore its pre-install entry (or remove the entry if
it was newly added) with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\AIGoodCrashRepair\tools\uninstall.ps1
```

Uninstall only changes `Plugins/PileChainRepair.asi`; it preserves every other
current archive entry. Both scripts refuse to run while the game or Settlers
United is open and reject an unexpected edit to the installed repair entry.

The repair changes in-memory bookkeeping. Load a test-only copy of the affected
save, allow the game to run briefly, confirm the log contains
`local cut success=true`, then save to a new slot. Keep the original save as a
backup.
