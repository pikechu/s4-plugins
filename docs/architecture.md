# Architecture

## Overview

S4 Campaign Tracker is planned as a 32-bit ASI plugin for **The Settlers IV: History Edition** running through **Settlers United**.

Its responsibilities are divided into four layers:

1. game integration;
2. victory and map classification;
3. persistent completion storage;
4. in-game completion marker rendering.

```text
+-----------------------------+
| CampaignCompletion.asi      |
+--------------+--------------+
               |
+--------------v--------------+
| Game Integration Layer      |
| - S4ModApi                   |
| - optional hooks            |
| - menu and game events      |
+--------------+--------------+
               |
+--------------v--------------+
| Completion Detection        |
| - local player victory      |
| - multiplayer exclusion     |
| - random map exclusion      |
| - stable map identification |
+--------------+--------------+
               |
+--------------v--------------+
| Completion Database         |
| - JSON storage              |
| - backup and atomic writes  |
| - manual correction         |
+--------------+--------------+
               |
+--------------v--------------+
| UI Marker Renderer          |
| - campaign buttons          |
| - map nodes                 |
| - scenario list entries     |
+-----------------------------+
```

## Completion rule

A completion record is created only when all of the following are true:

```text
Local player won
AND the game is not multiplayer
AND the map was not randomly generated
```

The plugin should therefore record:

- campaign missions;
- fixed single-player scenarios;
- other fixed single-player maps with a normal victory result.

It should ignore:

- multiplayer sessions;
- random-map sessions;
- defeats, aborted games, crashes, and normal returns to the menu.

## Stable content identifiers

Displayed mission names must not be used as unique identifiers because they can change with language settings.

Preferred identifier order:

1. normalized relative map path;
2. internal map identifier;
3. campaign identifier plus mission index;
4. menu page identifier plus control identifier as a last resort.

Example:

```json
{
  "stable_id": "campaign:new_world_maya:mission:2",
  "map_file": "maps\\campaign\\new_world\\maya02.map",
  "campaign_id": "new_world_maya",
  "mission_id": 2,
  "display_name": "Mayans 2"
}
```

## Menu compatibility

Campaign selection screens use multiple layouts, including:

- map-node layouts;
- vertical mission lists;
- faction-grouped mission buttons;
- fixed scenario list rows.

The plugin must not rely on one set of hard-coded screen coordinates. It should attempt to enumerate or observe native menu controls and derive each marker position from the corresponding control rectangle.

If direct menu control enumeration is unavailable, separate page adapters may be used behind a common interface.

```cpp
class ICampaignPageAdapter {
public:
    virtual ~ICampaignPageAdapter() = default;
    virtual bool MatchesCurrentPage() const = 0;
    virtual std::vector<MissionEntry> EnumerateEntries() = 0;
};
```

## Data storage

Planned runtime directory:

```text
CampaignCompletion/
|-- CampaignCompletion.ini
|-- completed_maps.json
|-- completed_maps.json.bak
`-- CampaignCompletion.log
```

JSON writes should use a temporary file followed by an atomic replacement. A valid existing database must not be overwritten when parsing or writing fails.

## Threading and performance

- `DllMain` must perform minimal work.
- Initialization should be delayed until Settlers United and S4ModApi are ready.
- Runtime JSON files must not be read or written every frame.
- Full process-memory scans must not run continuously.
- Rendering hooks must avoid blocking operations.

## Compatibility targets

Initial target environment:

- Windows 10 and Windows 11;
- The Settlers IV: History Edition;
- Settlers United;
- S4 version 3.15;
- `S4_Main.exe` file version `2.50.1516.0`;
- Win32/x86 build;
- S4ModApi;
- HD patch and common Settlers United ASI plugins.

## Planned implementation phases

1. inspect S4ModApi, symbols, and plugin loading behavior;
2. build a diagnostic ASI plugin;
3. implement reliable victory and map classification;
4. implement safe completion persistence;
5. add fixed-map list markers;
6. add campaign-page adapters;
7. add manual marking and configuration;
8. package a release build and documentation.
