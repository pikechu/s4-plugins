# AI Good Crash Repair

All implementation work for this feature lives below this directory:

- `src/` — plugin runtime and pile-chain repair core
- `tests/` — standalone core tests
- `tools/` — guarded Settlers United installer
- `backups/` — installer-owned backups (created on first installation)
- `repro/` — exact crashing map/save fixture, trace, hashes, and instructions

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

The nonempty economy-sector indices are cached at map initialization, and the
visited-entity workspace is reused between ticks to avoid scanning the full
sector pointer table and allocating a large marker array on every tick.

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
