# S4 Campaign Tracker

An in-game completion tracking plugin for **The Settlers IV: History Edition + Settlers United**.

S4 Campaign Tracker records completed single-player missions and displays completion indicators directly in the game UI.

The goal is to provide a unified progress tracker for all Settlers IV campaigns, fixed scenarios, and single-player missions.

---

## Features

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

## Installation

Copy the plugin files into the Settlers IV installation directory:

```
TheSettlers4/
|
|-- CampaignCompletion.asi
|
`-- CampaignCompletion/
    |-- CampaignCompletion.ini
    |-- completed_maps.json
    `-- CampaignCompletion.log
```

Example:

```
F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4
```

Launch the game through Settlers United.

---

## Development Roadmap

### Phase 1 - Research

- [x] Environment analysis
- [x] S4ModApi detection
- [ ] API investigation
- [ ] Symbol analysis

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
