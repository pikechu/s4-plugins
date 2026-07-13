# Phase 2.5 Settlers United Lua Map Identity Design

Date: 2026-07-13
Target: The Settlers IV History Edition through Settlers United
Status: Approved design; implementation not started

## Context

Phase 2.4 proved that the approved fixed-map call site at RVA `0x000FEFA5`
was not traversed by a controlled Single/Aeneas launch. The live game trace
recorded `adapter-entered=0` and `map-init-association=no-candidate`, while the
Hook, all 75 public listeners, and the diagnostic runtime stopped cleanly and
the game remained responsive. Selecting another private game call site without
new evidence would repeat the same risk.

Design-time research found a public Settlers United alternative. The SU Lua
library documents `SU.Game.GetMapName()` and
`SU.Game.GetMapNameRelativePath()` from library version 0.2.0 onward. The
installed `SettlersUnited.asi` contains both function names. S4ModApi publishes
`AddLuaOpenListener`, `AddMapInitListener`, and `AddTickListener`; its official
Lua example invokes Lua from the Tick callback. These facts support a bounded,
read-only C++-to-Lua bridge without another private game Hook.

Primary references:

- [SU.Game.GetMapName](https://github.com/SettlerWiki/Settlers-4-Lua-API-DE/blob/main/su-library-functions/su.game/su.game.getmapname.md)
- [SU.Game.GetMapNameRelativePath](https://github.com/SettlerWiki/Settlers-4-Lua-API-DE/blob/main/su-library-functions/su.game/su.game.getmapnamerelativepath.md)
- [S4ModApi Lua example](https://github.com/nyfrk/S4ModApi/blob/master/Examples/LuaExample/LuaExample/dllmain.cpp)
- [S4ModApi Lua-open Hook](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CLuaOpenHook.cpp)
- [S4ModApi map-init Hook](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CMapInitHook.cpp)
- [Settlers IV victory-condition documentation](https://github.com/SettlerWiki/Settlers-4-Lua-API-DE/blob/main/tutorials/aller-anfang/siegbedingungen-anpassen.md)

## Goal

Build a Win32 diagnostic ASI that uses only public S4ModApi callbacks and
read-only SU Lua function calls to associate the active fixed-map list with the
loaded map name and relative map path. A controlled Single/Aeneas launch must
produce one confirmed identity association before any three-list validation is
resumed.

## Non-goals

- Do not detect or persist completion.
- Do not observe, replace, or invoke victory-condition handlers.
- Do not draw completion markers.
- Do not select, install, or retain a private game or SU Hook.
- Do not modify Lua globals, tables, functions, scripts, or return values.
- Do not read an absolute map path or enumerate arbitrary Lua state.
- Do not modify a game, map, save, campaign, or Settlers United data file.
- Do not support non-SU launches in this phase.
- Do not run the Single, Multiplayer, and Custom live sequence until the
  Single/Aeneas gate succeeds.

## Authorized file and process boundaries

The existing user authorization remains unchanged:

- game and Settlers United files are read-only except project-owned
  `CampaignCompletion*.asi`, the `CampaignCompletion` configuration directory,
  and guarded exact replacement of `Plugin_SU.zip` after immutable-backup
  verification;
- runtime traces are created only below the configured project directory with
  exclusive creation and no overwrite or append;
- the user starts, navigates, tests, and closes the game and Settlers United;
- the plugin and development tools never terminate either process;
- deployment never proceeds while the game or Settlers United is running;
- normal writes made by the game or Settlers United remain outside plugin
  control.

The immutable original archive must retain SHA-256
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

## Chosen architecture

Phase 2.5 removes the Phase 2.3/2.4 call-site patch from the diagnostic binary.
The old Hook sources may remain in the repository as historical research, but
the 0.2.5 ASI target does not compile, link, construct, or start them. The
packaged configuration reports `FixedMapLoadHook=0` and `HookCount=0`.

The runtime uses five observer families:

1. existing UI-frame listeners identify the active menu page;
2. the existing mouse listener attributes an explicit fixed-map tab click to
   Single, Multiplayer, or Custom;
3. `AddLuaOpenListener` advances the Lua-state generation;
4. `AddMapInitListener` creates one pending map-identity session;
5. `AddTickListener` performs bounded SU function discovery and read-only
   calls on the game thread.

No background thread calls a Lua API. The bootstrap thread owns lifecycle only.
All Lua access occurs inside the S4ModApi Tick callback, matching the official
example's execution model.

## Components

### `ILuaMapBridge` and `S4LuaMapBridge`

`ILuaMapBridge` is the testable interface for one read attempt. It returns
independent results for map name and relative path with explicit status values.
The production `S4LuaMapBridge` is the only component permitted to call the
Lua 3.2 functions exposed by S4ModApi.

For each SU function it:

1. starts `lua_beginblock()`;
2. reads global `SU`, then field `Game`, then the requested function with
   `lua_getglobal` and `lua_getfield`/`lua_gettable`;
3. validates the intermediate objects and final callable;
4. calls the zero-argument function with `lua_callfunction`;
5. validates `lua_getresult(1)` as a string;
6. copies at most 512 bytes before leaving the callback;
7. ends `lua_endblock()` on every path.

The reader never exposes or retains a `lua_Object`, Lua string pointer, table,
or stack value. It never calls mutation or script-execution APIs, including
`lua_setglobal`, `lua_rawsetglobal`, `lua_settable`, `lua_rawsettable`,
`lua_dostring`, `lua_dofile`, or `lua_dobuffer`.

### `LuaMapSession`

`LuaMapSession` owns deterministic event ordering and retry policy. Its state
contains a monotonically increasing Lua generation, a monotonically increasing
map-init session ID, optional fixed-map list attribution, binding state,
deadline, attempt count, first successful values, and terminal result.

Generation rules are explicit:

- LuaOpen before MapInit: the new session binds to the current generation;
- MapInit before the first LuaOpen: the session stays unbound and binds once to
  the first following generation;
- a second LuaOpen after a session is bound invalidates that session as stale;
- results from an old generation never carry into a new one;
- MapInit consumes and replaces any earlier unresolved session.

The Tick callback attempts a read only when all of these are true:

- a nonterminal session exists;
- it is bound to the current Lua generation;
- S4ModApi reports the in-game screen;
- at least 50 milliseconds have passed since the previous attempt;
- neither the five-second deadline nor the 64-attempt cap has been reached.

Missing `SU`, `Game`, or either function is treated as a transient load-order
condition until the bounded deadline. A Lua call error, invalid object type,
invalid returned value, or conflicting successful value is terminal for the
session. Once both values are confirmed, no further Lua calls occur for that
session.

### `MapIdentityCoordinator`

The coordinator combines the fixed-map list epoch and a terminal Lua session.
It never infers list kind from a path string. A confirmed identity requires:

- an explicit, unexpired Single, Multiplayer, or Custom list epoch;
- one MapInit belonging to that epoch;
- a nonempty validated map name;
- a nonempty validated relative path;
- both values obtained from one Lua generation and map-init session.

The validated relative path becomes the stable map key. The display name is
kept separately. Name-only success is diagnostic evidence and is never a
stable identity.

### `S4Listeners`

`S4Listeners` registers LuaOpen and Tick in addition to the existing public
listener set. Every successful handle is retained in registration order and
removed in reverse order. All callbacks use the existing callback gate. Stop
first prevents new leases, waits for active callbacks, removes all handles,
then releases its S4ModApi reference.

LuaOpen performs state bookkeeping only. MapInit creates the pending session
but does not call Lua. Tick delegates at most one bounded read attempt to the
session and reader.

### `DiagnosticRuntime`

The runtime owns the reader, session, coordinator, listeners, logger, and
exclusive trace. It has no `FixedMapLoadHook`, patch backend, adapter, or
original-call forwarding responsibility in the 0.2.5 target.

Controlled stop order is:

1. mark the identity coordinator disabled;
2. close the listener callback gate and remove every listener in reverse order;
3. write the listener counts and controlled-stop trace result;
4. release S4ModApi;
5. close the trace and logger;
6. leave the ASI loaded but inert.

## Value validation and normalization

Lua strings are copied as byte sequences first. Empty values and values longer
than 512 bytes are rejected. Embedded NUL cannot be accepted as part of the
logical value. Control characters, CR, LF, and DEL are rejected so a returned
value cannot inject a trace record.

The production reader converts the copied game-code-page string to a Windows
wide string for path validation, then converts display output to UTF-8. A
conversion failure is terminal.

The relative path validator:

- converts `/` separators to `\`;
- collapses repeated separators and `.` components;
- rejects an empty result;
- rejects drive-qualified, drive-relative, rooted, UNC, and device paths;
- rejects every `..` component;
- does not require a `.map` suffix because built-in resources may use another
  representation;
- uses ordinal case-insensitive comparison for stable-key equality while
  retaining original casing for display.

The plugin never combines the returned relative value with the installation
root and never opens the named map file.

## Result model

Function-read statuses are:

- `success`;
- `su-table-missing`;
- `game-table-missing`;
- `function-missing`;
- `call-error`;
- `non-string`;
- `empty`;
- `too-long`;
- `control-character`;
- `encoding-failure`;
- `absolute-path`;
- `path-traversal`;
- `value-conflict`;
- `stale-generation`;
- `timeout`.

Association results are:

- `confirmed`: both values valid in one attributed session;
- `name-only`: the name succeeded but the relative value did not before the
  bounded deadline;
- `no-list-epoch`: MapInit was not attributable to an explicit fixed-map tab;
- `lua-unavailable`: the Lua generation or SU objects never became usable;
- `invalid-value`: a terminal value validation failure occurred;
- `conflict`: a successful value changed within one session;
- `stale-generation`: Lua reopened before resolution.

Exactly one terminal association is emitted per MapInit session.

## Dedicated project trace

Phase 2.5 reuses exclusive `CREATE_NEW` trace creation below the configured
existing non-reparse project directory. The trace accepts only these prefixes:

```text
lua-open-generation=
map-init-session=
su-map-name-status=
su-map-relative-status=
su-map-name=
su-map-relative=
identity-association=
controlled-stop-flush=
```

Value records are emitted only after byte, control-character, encoding, and
path validation. They contain the map name or relative path returned by SU,
never an absolute path, pointer, Lua object, stack content, arbitrary memory,
module inventory, or unrelated UI/mouse data. General diagnostic logging stays
outside the Lua call block and rate-limited callback path.

## Alternatives not chosen

### Reverse-engineer a native function in `SettlersUnited.asi`

The installed binary contains both map-name function strings, but no supported
native ABI or export was found. Calling a private function would bind the plugin
to a specific SU build and reintroduce private-address and calling-convention
risk. It remains a research fallback only if the public Lua bridge is proven
unavailable.

### Hook `CreateFileW`

The open-source Settlers4 Texture Changer demonstrates a process-wide
`CreateFileW` Hook. That Hook must prevent recursive file calls and competes at
a shared operating-system entry point. It observes much more than the selected
map, has weak attribution, and creates avoidable compatibility risk with SU and
other plugins. It is rejected for this phase.

### Observe or replace victory-condition Lua handlers

The documented `Events.VICTORY_CONDITION_CHECK` mechanism can replace a map's
normal handler, and `Game.DefaultGameEndCheck()` can produce game-end behavior.
Using either for identity would change scope and could alter gameplay. Victory
research is deferred until map identity is independently proven.

## Automated verification

Development follows RED/GREEN commits on `main`, as explicitly authorized.
The Win32 GitHub Actions workflow is authoritative because local MSVC Win32 is
unavailable.

Unit tests cover:

- LuaOpen before MapInit and MapInit before the first LuaOpen;
- a second LuaOpen invalidating a bound session;
- missing SU table, Game table, and each function;
- call error, non-string, empty, over-limit, control-character, and encoding
  failures;
- valid relative paths, slash normalization, repeated separators, and dot
  components;
- drive, rooted, UNC, device, and traversal path rejection;
- name-only timeout and full timeout;
- repeated identical success and conflicting success;
- one terminal association per session;
- no read after confirmation, timeout, disable, or controlled stop;
- callback gate drain and exact reverse listener removal;
- unique trace creation, prefix allowlist, and value sanitization.

Static policy tests require:

- `Version=0.2.5`, `FixedMapLoadHook=0`, and `HookCount=0`;
- no Hook backend or Hook source in the 0.2.5 ASI target;
- no `hlib::CallPatch`, `hlib::JmpPatch`, `hlib::NopPatch`, MinHook, Detours,
  or direct patch write in production target sources;
- no forbidden Lua mutation or script-execution API in the production reader;
- all Lua calls confined to the Tick callback path;
- packaged `CaptureTraceRoot` empty;
- Win32 PE machine `0x014c` and the existing ASI exports preserved.

The complete workflow must pass twice before freezing the artifact.

## Guarded deployment and live acceptance

Before deployment the user closes the game and Settlers United normally.
Read-only process checks must find neither process. The guarded installer
verifies the immutable original archive, replaces only the project ASI entry,
and reports the embedded ASI hash. The authorized deployed INI differs from the
frozen INI only by the approved project trace root.

The single live gate is:

1. launch normally through Settlers United and wait at the main menu;
2. enter Single Player Maps;
3. explicitly click calibrated Single tab ID `2449`, even if already selected;
4. select Aeneas and enter the map;
5. remain in the map while the project trace is read;
6. require a current Lua generation, one MapInit session, successful name and
   relative-path reads, and `identity-association=confirmed`;
7. create only the authorized controlled-stop file;
8. require every listener removed with zero failures and
   `controlled-stop-flush=success`;
9. ask the user to confirm that the game remains responsive.

GO means the explicit Single epoch, MapInit, SU map name, SU relative path, and
one Lua generation all agree. Any other result is NO-GO with the exact recorded
boundary. A NO-GO does not restore the old Hook or select a new private call
site inside this phase. A GO permits the already planned Single, Multiplayer,
and Custom three-list live sequence without adding a game Hook.

## Completion criteria

Phase 2.5 is complete only when:

- the 0.2.5 target contains no process patch;
- two full Win32 CI runs pass;
- the frozen package and guarded deployment hashes are recorded;
- the Single/Aeneas trace records a terminal result;
- controlled stop removes all public listeners and flushes the trace;
- the user reports post-stop responsiveness;
- an evidence report records exactly one GO or NO-GO conclusion without
  claiming victory, persistence, completion, or marker behavior.
