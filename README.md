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

Current status: completion persistence and fixed-map marker rendering are
implemented and live-validated. Phase 5C restored compatibility with historical
`0.5.0` records without rewriting them, then passed a four-record Phoenix
victory, atomic-backup, same-process marker refresh, and normal-shutdown
acceptance. See the [Phase 5C GO report](docs/research/phase-5c-database-compatibility-recovery-report.md).

Phase 6A is an **EVIDENCE GAP**. Its structurally read-only Dark Tribe
diagnostic proved safe zero-write operation and observed one public mission
control plus the exact launched `identity.relative`, but the public callback
behaved as a sparse state-change stream. The candidate therefore did not form
a stable no-hover menu snapshot or its own click-to-session association, and no
campaign mapping or marker is authorized. See the [Phase 6A report](docs/research/phase-6a-dark-tribe-campaign-menu-forensics-report.md),
[design](docs/superpowers/specs/2026-07-16-phase-6a-dark-tribe-campaign-menu-forensics-design.md),
and [static evidence](docs/research/phase-6a-dark-tribe-campaign-menu-static-evidence.md).

Phase 6A.1 is **GO for the public association subproblem**. Its bounded
page-residency sparse cache retained mission controls across empty callbacks,
and the live run associated exact control `2083` through campaign MapInit
session 1 to the same-session confirmed `Map\Campaign\dark03.map`, with zero
database changes. This does not add an internal reader or authorize campaign
markers; the complete no-hover 12-mission catalog remains an evidence gap. See
the [Phase 6A.1 design](docs/superpowers/specs/2026-07-16-phase-6a-1-public-sparse-campaign-association-design.md)
and [GO report](docs/research/phase-6a-1-public-sparse-campaign-association-build-report.md).

Phase 6A.2 static evidence supports a smaller complete-catalog design without
a private mutable menu reader: an exact-version-gated immutable table for
controls 2081–2092 and `Map\Campaign\dark01.map` through `dark12.map`,
cross-checked by the public page-21 callback. Implementation and deployment
remain unapproved. See the [static evidence](docs/research/phase-6a-2-dark-tribe-static-catalog-evidence.md)
and [design](docs/superpowers/specs/2026-07-16-phase-6a-2-static-campaign-catalog-design.md).

Phase 6B offline evidence proves the exact path-format families and PDB state
ownership for original, Add-on, Mission CD, New World, and New World 2
campaigns. It does not promote incomplete non-Dark control catalogs to GO.
The unified design keeps each descriptor independently gated and replaces
per-mission repetition with one sequential single-process catalog pass plus at
most one dynamic anchor per distinct proven menu implementation. The `0.8.0`
read-only public catalog calibration candidate is now implemented and passed
the authoritative Windows CI and artifact audit; deployment remains
unapproved. See the [static evidence](docs/research/phase-6b-all-campaign-static-catalog-evidence.md),
[batch design](docs/superpowers/specs/2026-07-16-phase-6b-unified-campaign-catalog-batch-design.md),
and [candidate audit](docs/research/phase-6b-all-campaign-public-catalog-candidate-audit.md).

The first Phase 6B live catalog batch accepted sparse capture on pages 11–21
and exposed selector companion layers on 3/4 and the shared 5/6 New World page
set. Candidate `0.8.2` combined deterministic composed-page ownership with a
bounded fail-closed startup retry, passed authoritative Windows CI, and then
passed the focused live retest: pages 3, 4, and 6 all produced successful
snapshots without clicks or launches. Normal-shutdown postflight preserved the
database and backup byte-for-byte. Phase 6B is GREEN for public catalog and
page-isolation calibration, but does not authorize inferred mission mappings or
campaign markers. See the
[initial batch and fix audit](docs/research/phase-6b-initial-batch-composed-page-fix-audit.md).

Development feedback policy:

- keep intermediate RED test commits local;
- push a complete GREEN checkpoint instead of each test-first intermediate;
- use the full Windows CI as the authoritative Win32, policy, package, and PE32
  gate before deployment.

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

- [x] Victory detection
- [x] Map identification
- [x] Completion database

### Phase 3 - UI Integration

- [ ] Campaign menu markers
- [x] Scenario list markers
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
