# S4 Campaign Tracker

An in-game completion tracking plugin for **The Settlers IV: History Edition + Settlers United**.

The project is being developed to record completed single-player missions and
display completion indicators directly in the game UI.

The goal is to provide a unified progress tracker for all Settlers IV campaigns, fixed scenarios, and single-player missions.

---

## Features

The following list describes the intended release feature set, not the current
diagnostic build:

- In-game completion indicators
- Automatic victory detection
- Campaign progress tracking
- Fixed single-player scenario tracking
- Persistent completion database
- Language-independent mission identification
- Manual completion marking
- JSON-based local storage

Supported content:

- The Three Races
- The Dark Tribe
- The New World
- The Trojans
- Great Crusades
- Other single-player campaigns
- Fixed single-player scenarios

---

## Supported Environment

Target environment:

- The Settlers IV: History Edition
- Version 3.15
- S4_Main.exe
- File version: 2.50.1516.0
- Settlers United
- Windows 10 / Windows 11
- Win32 executable

---

## Completion Rules

The plugin records completion when:

```
Local Player Victory
        +
Not Multiplayer
        +
Not Random Generated Map
        =
Record Completion
```

Recorded:

- Campaign missions
- Fixed single-player maps
- Scripted missions with victory conditions

Ignored:

- Multiplayer games
- Random generated maps

---

## Technical Design

Implemented as:

- Win32 ASI plugin
- C++17
- S4ModApi integration
- Optional game hooks when required

Architecture:

```
+----------------------+
| CampaignCompletion   |
|       .asi           |
+----------+-----------+
           |
           |
+----------v-----------+
| Victory Detection    |
+----------+-----------+
           |
           |
+----------v-----------+
| Completion Database  |
+----------+-----------+
           |
           |
+----------v-----------+
| UI Completion Marker |
+----------------------+
```

---

## Phase 2.1 Diagnostic Build

`CampaignCompletionDebug` is a diagnostic-only bootstrap. It inventories loaded
modules and records events delivered through public S4ModApi listeners. It does
not detect victory, save completion state, create completion JSON, or render
completion markers. It installs no internal game hooks.

Phase 2.1 attributes nested menu pages to their exact public S4ModApi callback
and supports a controlled runtime stop that waits for in-flight callbacks and
removes listeners in reverse registration order.

The CI artifact has this layout:

```
TheSettlers4/
|
|-- Plugins/
|   `-- CampaignCompletionDebug.asi
|
`-- CampaignCompletion/
    `-- CampaignCompletionDebug.ini
```

Settlers United synchronizes the game `Plugins` directory before launch. With
the owner's explicit authorization, Phase 2.1 therefore adds only
`Plugins/CampaignCompletionDebug.asi` inside this archive after backing it up:

```
C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip
```

Both `S4_Main.exe` and the Settlers United application must be closed before
install or restore. From the repository root, install a CI-built ASI with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\install_settlers_united_artifact.ps1 -AsiPath "F:\path\to\CampaignCompletionDebug.asi"
```

The installer creates and verifies
`research/backups/settlers-united/Plugin_SU.zip.original` plus JSON metadata,
preserves the hash of every original ZIP entry, and refuses an unrecognized
installed archive. Proprietary backup binaries and generated metadata under
`research/backups/` are ignored by Git.

Restore the byte-identical original archive with both applications closed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\restore_settlers_united_artifact.ps1
```

When the game runs, the diagnostic plugin may create
`CampaignCompletion/CampaignCompletion.log`. To quiesce the loaded diagnostic
plugin without unloading its ASI, create this one-shot file in the game
directory:

```powershell
New-Item -ItemType File -Force "F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.stop"
```

The plugin deletes the request after consuming it, rejects new callbacks, waits
for active callbacks, removes all listeners, releases S4ModApi, and remains
inert. To uninstall persistently, run the restore command; then, if desired,
delete only this project's diagnostic configuration and log under
`CampaignCompletion`. Do not copy research scripts or build files into the game
directory.

Example game location:

```
F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4
```

Launch the game through Settlers United.

---

## Development Roadmap

Current status: Phase 1 research and the Phase 2 diagnostic bootstrap are
complete; Phase 2.1 hardening is under live validation. No completion-tracking
Release plugin exists yet.

Research and planning documents:

- [Approved design specification](docs/superpowers/specs/2026-07-13-campaign-completion-design.md)
- [Phase 1 execution plan](docs/superpowers/plans/2026-07-13-phase-1-investigation.md)
- [Phase 1 investigation report](docs/research/phase-1-investigation-report.md)
- [Phase 2 diagnostic bootstrap plan](docs/superpowers/plans/2026-07-13-phase-2-diagnostic-bootstrap.md)
- [Phase 2 diagnostic bootstrap report](docs/research/phase-2-diagnostic-bootstrap-report.md)
- [Phase 2.1 hardening design](docs/superpowers/specs/2026-07-13-phase-2-1-diagnostic-hardening-design.md)
- [Phase 2.1 hardening plan](docs/superpowers/plans/2026-07-13-phase-2-1-diagnostic-hardening.md)

### Phase 1 - Research

- [x] Environment analysis
- [x] S4ModApi detection
- [x] API investigation
- [x] Symbol analysis

### Phase 2 - Core System

- [ ] Victory detection
- [ ] Map identification
- [ ] Completion database

### Phase 3 - UI Integration

- [ ] Campaign menu markers
- [ ] Scenario list markers
- [ ] Support different campaign layouts

### Phase 4 - Release

- [ ] Stable release
- [ ] Documentation
- [ ] Installer package

---

## Compatibility Goals

The plugin should:

- Avoid modifying original game files
- Avoid modifying save files
- Avoid affecting multiplayer
- Avoid affecting random maps
- Work with Settlers United
- Work with HD patches

---

## Credits

Created for the Settlers IV community.

Thanks to:

- Settlers United developers and contributors
- S4ModApi developers and contributors
- The Settlers IV modding community
- ChatGPT for project planning, architecture design, and development assistance

---

## License

This project is licensed under the MIT License.

See [LICENSE](LICENSE) for details.
