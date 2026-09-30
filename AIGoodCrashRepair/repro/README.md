# Crash reproduction fixture

This directory contains the exact map/save pair that reproduced the AI goods
crash investigated on 2026-07-18.

## Files

| File | Size | SHA-256 | Purpose |
|---|---:|---|---|
| `Pathes.map` | 1,088,488 bytes | `a3cc969f787793c143560740db019d5b66e5c619b84455e158191c9fff191108` | Required base map |
| `QuickSave.crash-original.sav` | 1,864,954 bytes | `6aa3fc56e05f3a53c990824168b3ce482d41f4b79cd114abb98285273d8a9035` | Original save containing the bad runtime state |
| `Trace100.txt` | 40,936 bytes | `ab0eb9b61ba9324ea828c1b086ad405083cbe22f22f681bfb75ea40840168e15` | Reference crash trace |

`Pathes.map` by itself is not corrupt. The reproducible fault is the saved
economy/pile state in `QuickSave.crash-original.sav`; the map is included so
the fixture is self-contained.

## Source locations

The files were copied without modifying their sources:

- Map:
  `F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\Map\Singleplayer\Pathes.map`
- Save:
  `C:\Users\paprika\Documents\TheSettlers4\Save\QuickSave.sav`
- Trace:
  `C:\Users\paprika\Documents\TheSettlers4\Log\Trace100.txt`

The copied files were verified against the source files with SHA-256.

## Required executable

The observed addresses and the repair admission apply only to:

- `S4_Main.exe` version: `2.50.1516.0`
- SHA-256:
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`

## Reproduction

1. Copy `Pathes.map` to the game's `Map\Singleplayer` directory.
2. Copy `QuickSave.crash-original.sav` to
   `%USERPROFILE%\Documents\TheSettlers4\Save` under a test-only filename.
3. Start the admitted game version and load the copied save.
4. Let the game run at normal speed.

Without an effective repair, the game crashes shortly after loading with:

- exception: `0xc0000005`
- function: `sub_D4280+39`
- faulting read: address `0x00000000`
- pile entity ID: `6302` (`0x189E`)

`Trace100.txt` records this exact failure.

The original archive-level rollback snapshot is retained at
`../backups/Plugin_SU.zip.pre-pile-chain-repair` as historical evidence. The
current installer no longer restores that whole archive; it snapshots and
rolls back only the current installation transaction, preserving newer
Campaign Marker and other plugin entries.

## Repair acceptance criteria

With `PileChainRepair` v0.3 or later:

- the log must show `bootstrap version=0.3.1`;
- a fatal dangling node should produce `local cut success=true`;
- the log must not contain `rebuild success=true`;
- civilian transport must continue working;
- the game must remain stable for several minutes;
- any test output must be saved to a new slot, never over the fixture.

## Contaminated saves not included

The following saves were written after the rejected v0.2 full-rebuild
experiment cleared economy-sector pile heads. They are not valid regression
fixtures for the local-cut repair:

- `pathes 2_49.sav`
- `pathes 3_09.sav`
- `pathes 3_22.sav`
- the corresponding `AutoSave.sav`
