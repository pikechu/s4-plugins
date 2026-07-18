# Phase 3 Victory Diagnostics Design

Date: 2026-07-13
Status: Approved in conversation; awaiting written-spec review
Target: The Settlers IV History Edition with Settlers United

## Context

Phase 2.5 proved that one map-init session and one Settlers United Lua
generation can yield a stable map name and relative map path through the public
S4ModApi callbacks and read-only `SU.Game` functions. The live Aeneas test
ended with a GO result and a controlled 77/77/0 listener shutdown.

The next requirement is to determine whether the local player won. This phase
is diagnostic only: it must prove the decision inputs and negative controls
before completion persistence or menu markers are implemented.

The public S4ModApi interface exposes `GetLocalPlayer`, `HasPlayerLost`, GUI
page callbacks, GUI-element draw callbacks, map-init, Lua-open, and tick
listeners. It does not expose an `OnVictory`, `OnGameEnd`, `IsMultiplayer`, or
`IsRandomMap` listener/getter. `GameDefaultGameEndCheck` calls the game's Lua
end check and is not a read-only status query.

Primary references:

- [S4ModApi interface](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/S4ModApi.h)
- [S4ModApi scripting implementation](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CS4Scripting.cpp)
- [Settlers IV victory-condition documentation](https://github.com/SettlerWiki/Settlers-4-Lua-API-DE/blob/main/tutorials/aller-anfang/siegbedingungen-anpassen.md)
- [Phase 2.5 evidence report](../../research/phase-2-5-su-lua-map-identity-report.md)

## Goal

Build a bounded Win32 diagnostic ASI that proves all of the following:

1. the active map and its original launch/load source are known;
2. online multiplayer and random-map sessions are excluded;
3. offline single-player sessions remain eligible even when the selected map
   came from the `Multiplayer Maps` list;
4. the public after-game GUI distinguishes local victory, local defeat, and
   voluntary exit;
5. new-game and load-game paths produce the same safe classification; and
6. one map session produces at most one diagnostic terminal decision.

## Non-goals

- Do not write `completed_maps.json` or any completion database.
- Do not render a completion marker.
- Do not unlock or mutate campaign state.
- Do not call `GameDefaultGameEndCheck`.
- Do not replace, wrap, or invoke Lua victory-condition handlers.
- Do not modify a game, map, save, campaign, or Settlers United data file.
- Do not install a private internal game Hook in the approved public-observer
  implementation.
- Do not claim that a map is completed merely because the local player has not
  lost.

## Eligibility rule

The map-list label and the game-session mode are separate facts. Eligibility
is based on session origin, not on whether a map was authored for one or more
players.

Eligible examples include:

- `Single Player Maps`;
- `Multiplayer Maps` launched from the single-player flow;
- `Custom Maps` launched from the single-player flow;
- campaigns, Dark Tribe, Great Crusades, Tutorial, Add On, and Mission CD;
- load games whose original source is proven to be eligible.

Exactly two source families are excluded:

- online multiplayer sessions; and
- random maps, whether launched or loaded through a single-player or
  multiplayer flow.

Unknown source is not an exclusion verdict. It is an incomplete diagnostic
result and a phase-completion blocker for the covered load-game matrix.

## Architecture

The diagnostic keeps map identity, launch origin, settlement UI, and the final
decision as separate evidence families. No family may infer facts owned by a
different family.

### `LaunchOriginTracker`

`LaunchOriginTracker` observes public S4ModApi screen callbacks and maintains a
bounded navigation epoch until `MapInit` consumes it. It distinguishes at
least:

- single-player campaign and map flows;
- single-player fixed, multiplayer-authored, random, and custom map selectors;
- online multiplayer selectors and lobby;
- campaign, ordinary-map, and multiplayer load-game screens.

The relevant public page distinctions include:

- `S4_SCREEN_SINGLEPLAYER_MAPSELECT_MULTI` versus the
  `S4_SCREEN_MULTIPLAYER_*` flow;
- `S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM` and
  `S4_SCREEN_MULTIPLAYER_MAPSELECT_RANDOM`;
- `S4_SCREEN_LOADGAME_CAMPAIGN`, `S4_SCREEN_LOADGAME_MAP`, and
  `S4_SCREEN_LOADGAME_MULTIPLAYER`.

An epoch expires, is replaced by a newer unambiguous navigation decision, or
is consumed by one `MapInit`. Ambiguous, missing, expired, or conflicting
navigation remains `unknown`; it is never guessed from a display name.

### `ActiveMapIdentity`

`ActiveMapIdentity` generalizes the Phase 2.5 Lua session so a fixed-list tab
is optional. After one `MapInit`, it binds to one Lua generation and performs
only these read-only calls on the game thread:

- `SU.Game.GetMapName()`;
- `SU.Game.GetMapNameRelativePath()`.

The existing length, encoding, relative-path, generation, retry, deadline, and
conflict checks remain in force. A confirmed identity requires nonempty valid
name and path values from the same generation and map-init session.

For new fixed-map launches, an explicit list epoch remains useful origin
evidence. Campaign and load-game identity must not be rejected merely because
that fixed-list epoch is absent.

### Load-game source recovery

Loading is a first-class path, not a permanent `unknown` fallback. Source
recovery uses three evidence levels:

1. the public load-game page provides the coarse campaign, ordinary-map, or
   online-multiplayer class;
2. post-load `MapInit` plus the SU name and relative path recover the original
   map identity without relying on state retained before the save; and
3. an ordinary-map save must be further classified as fixed/custom or random.

The random-map distinction is established empirically by paired fresh and
loaded samples. If the SU values expose a stable documented distinction, the
public value is used. If they do not, the next step is limited to copying only
user-created or explicitly designated test saves into the project and
performing read-only metadata comparison. Any prospective save-field parser
requires multiple positive and negative samples, fixed bounds, structural
invariants, and an independent review before production use.

If save metadata also fails, the phase stops with an evidence report. A
read-only internal random-map probe requires a separate design and explicit
approval; it is not automatically added by this design.

### `SettlementUiProbe`

The after-game summary page is a trigger, not proof by itself. Previous live
testing showed that voluntary exit can also reach
`S4_SCREEN_AFTERGAME_SUMMARY`.

When a session transitions from `S4_SCREEN_INGAME` to the after-game summary,
`SettlementUiProbe` opens one short, count-limited capture window. It observes
public GUI-element drawing data and records only:

- stable element/control identifiers;
- stable resource/effect-related numeric fields that are already present in
  the public callback structure;
- the bounded page transition sequence and timestamps;
- the local player identifier and `HasPlayerLost(localPlayer)` result; and
- when necessary, the length and cryptographic hash of a bounded text value,
  never arbitrary original UI text.

The probe does not predeclare a victory element. Calibration first captures
voluntary exit, normal victory, and normal defeat. A class signature is
accepted only when it is stable within that class and does not appear in the
negative controls. A decision uses a combination of features, not one element
or `HasPlayerLost` alone.

### `VictoryDecision`

`VictoryDecision` combines immutable snapshots from the other components. Its
terminal results are:

- `victory-confirmed`: eligible source, confirmed map identity, calibrated
  victory UI, valid local player, and no conflicting loss signal;
- `defeat-observed`: calibrated defeat UI;
- `exit-observed`: calibrated voluntary-exit UI;
- `excluded-online-multiplayer`: proven online multiplayer source;
- `excluded-random-map`: proven random-map source; and
- `inconclusive`: missing, expired, uncalibrated, or conflicting evidence.

For excluded sources, an observed victory UI cannot upgrade the result. For an
eligible source, `HasPlayerLost == false` cannot create a victory without the
calibrated victory UI. Each map-init session may emit exactly one terminal
decision.

## State machine

The high-level session states are:

```text
waiting-for-origin-and-map-init
  -> waiting-for-lua-identity
  -> active-game-session
  -> settlement-capture
  -> terminal-decision
```

Events outside their valid state are ignored or produce a bounded diagnostic
reason. A second `MapInit` replaces any unresolved earlier session. A new Lua
generation invalidates a session already bound to an older generation. A
direct menu return without calibrated settlement UI is not victory.

## Diagnostic output

All phase output is confined to a new project directory:

```text
artifacts/phase-3-victory-diagnostics/
  <session-id>/
    origin.trace
    identity.trace
    settlement-ui.trace
    decision.trace
```

The trace implementation must enforce an explicit allowlist. It must not
record:

- absolute save or map paths;
- memory addresses or dumps;
- player names, chat, or unrelated text;
- completion/persistence claims; or
- unbounded GUI events.

Map output is limited to the already validated name and relative path. File
names are unique per phase and session and never overwrite previous phase
logs.

## Automated verification

Development follows test-driven RED/GREEN steps on `main`, as explicitly
authorized by the user. Unit tests cover at least:

- every supported menu-origin transition;
- the difference between an offline `Multiplayer Maps` launch and online
  multiplayer;
- random-map launch and load exclusion;
- epoch expiry, replacement, ambiguity, and one-time consumption;
- new-game and load-game identity without a required fixed-list epoch;
- Lua-generation invalidation and value conflicts;
- bounded settlement capture and feature normalization;
- victory, defeat, exit, excluded, and inconclusive decision tables;
- exactly one terminal result per map session;
- trace allowlists, event limits, and forbidden data; and
- controlled stop ordering and listener removal.

The authoritative build remains Win32 GitHub Actions with `/W4 /WX
/permissive-`. Two clean CI runs are required before deployment. The frozen
ASI must retain the existing PE machine, exports, exact executable admission,
listener ownership, and controlled-stop properties.

## Live calibration and validation

Live work is gated and user-driven:

1. deploy a calibration build only after CI and artifact verification;
2. capture a voluntary-exit settlement sample;
3. capture a normal-victory settlement sample;
4. capture a normal-defeat settlement sample;
5. prove that the three public GUI signatures are stable and disjoint;
6. freeze the accepted signature rules and rebuild;
7. validate an eligible fresh map and its corresponding load game;
8. validate fresh and loaded source cases for single-player fixed maps,
   single-player `Multiplayer Maps`, Custom Maps, campaigns, random maps, and
   online multiplayer saves as available in the required matrix;
9. controlled-stop the plugin, flush traces, remove all listeners, and ask the
   user to verify that the game remains responsive.

The user starts, navigates, saves, loads, wins, loses, exits, and closes the
game. The diagnostic never kills the game or Settlers United process.

## Deployment boundary

Deployment retains the previously authorized guarded archive workflow:

- the game must be closed before archive replacement;
- verify the exact current `Plugin_SU.zip` and immutable original backup;
- replace only the project-owned `CampaignCompletion*.asi` entry;
- do not alter Settlers United, S4ModApi, or any other plugin entry;
- record the archive and embedded-ASI SHA-256 values; and
- preserve a verified recovery path to the immutable original archive.

No other game-directory file may be created, modified, replaced, or deleted.
Normal writes made by the game itself during user-driven tests remain allowed.

## Failure handling and fallback

- Partial listener registration makes the diagnostic inert.
- Missing or conflicting origin, identity, or UI evidence produces
  `inconclusive`, never victory.
- An unclassified covered load-game case blocks a Phase 3 GO result and
  continues investigation; it is not silently dropped from scope.
- Public-GUI ambiguity ends the public-only experiment with a report. It does
  not automatically install `CStateVictoryScreen` or another internal Hook.
- Save analysis is restricted to copied, explicitly scoped test samples and
  never changes an original save.
- Controlled-stop failure blocks the next deployment.

## Rejected alternatives

### Public after-game page plus `HasPlayerLost` alone

Rejected because voluntary exit can reach the after-game page, and “not lost”
does not prove that a special mission victory condition completed.

### Calling `GameDefaultGameEndCheck`

Rejected because S4ModApi invokes the Lua `Game.DefaultGameEndCheck` function;
it is behavior, not a passive game-end getter.

### Wrapping `Events.VICTORY_CONDITION_CHECK`

Rejected because it mutates a map's Lua handler chain and can interfere with
map-specific victory behavior.

### Immediate `CStateVictoryScreen` Hook

Deferred. The user confirmed that victory, defeat, and voluntary-exit UIs are
visibly distinct, so public GUI calibration gets the first evidence gate. An
internal Hook is considered only under a separate approved design if the
public data cannot distinguish those screens reliably.

### Excluding the `Multiplayer Maps` list

Rejected because a multiplayer-authored map launched through the offline
single-player flow is eligible. Only online multiplayer session origin is
excluded.

### Leaving load-game origin permanently unknown

Rejected because loading is a common play path. Covered load-game cases must
recover source or remain an explicit phase blocker.

## GO / NO-GO criteria

Phase 3 is GO only when:

- public GUI evidence reproducibly distinguishes victory, defeat, and exit;
- at least one eligible fresh-map victory and its corresponding load-game
  victory are both classified correctly;
- the required origin matrix proves offline `Multiplayer Maps` eligible and
  online multiplayer plus random maps excluded, including load paths;
- no negative control emits `victory-confirmed`;
- two clean Win32 CI runs, artifact verification, guarded deployment, bounded
  traces, controlled stop, and user responsiveness confirmation succeed; and
- the report explicitly states that persistence and markers remain unbuilt.

Any unmet required item is NO-GO with the precise missing or conflicting
evidence. A NO-GO does not authorize an internal Hook or game-file mutation.
