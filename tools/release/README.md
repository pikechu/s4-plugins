# Settlers IV accepted plugin pair

Campaign Marker **0.13.4** + PileChainRepair **0.3.1**, accepted on 2026-09-30.

Unzip the complete package to a permanent local folder. Close the game and
Settlers United, then run PowerShell from the extracted folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -GameDirectory 'F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4'
```

Supply your actual game directory. It must contain the admitted
`S4_Main.exe` 2.50.1516.0 build (SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`). `-SettlersUnitedDirectory` defaults to
`C:\Program Files\Settlers United`. The script requests Windows administrator
permission when needed. Nothing installs merely by extracting or building the
package.

The installer verifies manifest hashes and the exact accepted ASI hashes,
updates only `Plugins/CampaignCompletionDebug.asi` and
`Plugins/PileChainRepair.asi` inside SU's
`resources/bin/s4_artifacts/Plugin_SU.zip`, and synchronizes those two files in
the selected game's `Plugins` directory. Other archive entries are checked
byte-for-byte by their uncompressed SHA-256. It checks that both applications
are closed before preparing and again before publishing. Other installers
must also stay closed for the duration of the operation.

## Backup and rollback

Installation prints `BackupDirectory`. The default is a new directory under
`%LOCALAPPDATA%\Settlers4Plugins\backups`; you may supply an explicit,
nonexistent `-BackupDirectory` instead. Keep this directory. It contains the
current SU archive snapshot, each original target entry and live ASI if
present, and `metadata.json` with exact locations, presence flags, and hashes.

Close both applications and restore that transaction with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rollback.ps1 -BackupDirectory 'C:\Users\yourname\AppData\Local\Settlers4Plugins\backups\the-directory-printed-by-install'
```

Rollback reconstructs the *current* archive with just the two target entries
restored or removed according to their state before installation. It retains
later changes to other plugins. It refuses rollback if either target ASI was
changed externally. If you install this pair repeatedly, roll back in reverse
order. Backups remain after success, rollback, or a failed transaction; a
failed restoration reports the recovery snapshot location.

These combination backups belong to the combination installer. Existing
Campaign Marker and PileChainRepair standalone installation metadata is not
updated. Avoid interleaving standalone and combination rollbacks; a
standalone rollback may refuse changed hashes, and replacing a complete old
archive would discard newer plugin changes.

## Configuration and acceptance

`config/CampaignCompletionDebug.ini.example` is reference material. This
bundle does not copy, replace, or change any existing INI, completion database,
log, map, or save. Existing accepted installations can keep their current
configuration. For a fresh setup, review the example, then manually copy it to
`<GameDirectory>\Plugins\CampaignCompletion\CampaignCompletionDebug.ini`
**only when that destination does not already exist**. The destination folder
may be created if needed. Existing configuration and completion data stay in
place. Do not overwrite an existing INI merely to install this pair.

`docs/pile-chain-acceptance.md` records the known crashing save, its repair,
save/reload, and transport acceptance. `docs/campaign-marker-audit.md` records
the marker geometry change and user acceptance. PileChainRepair admits only
the tested `S4_Main.exe` 2.50.1516.0 hash; the scope is that known fault and
observed sessions. Music diagnosis work is excluded from this bundle.

## Integrity and execution

`manifest.json` lists SHA-256 and sizes for every payload file. The ZIP `.sha256` file delivered beside the archive identifies the exported package. These hashes detect byte
changes; this package is not a signed distribution. Keep scripts, module,
manifest, payloads, and the original transaction backup together.

The game and SU archive receive the exact two accepted binaries. Config
examples and acceptance documentation are references only. No game binary,
user save, crash fixture, or proprietary SU archive is included.
