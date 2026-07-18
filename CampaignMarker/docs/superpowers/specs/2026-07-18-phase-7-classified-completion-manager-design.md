# Phase 7 classified completion manager design

Date: 2026-07-18 (Asia/Shanghai)

## Objective and authorization

Phase 7 adds a native Windows completion-manager window. It presents the
current marker state for every admitted campaign mission and every discoverable
fixed map, grouped by content family. The user can check or uncheck entries and
apply the complete edit as one atomic database transaction.

The current authorization covers design, offline implementation, constructed
tests, Windows CI, and artifact audit. It does not authorize deployment,
starting or controlling the game, opening the live manager, reading or writing
the live database, or modifying saves/campaign progress.

The independent uncommitted `PileChainRepair` work is outside Phase 7. Its
files and CMake/TestMain changes must be preserved and excluded from Phase 7
commits.

## User experience

`Ctrl+Shift+M` opens **Campaign Completion Manager** only while the public main
menu is active and no map session is running. The shortcut is edge-triggered
with `GetAsyncKeyState`; it installs no keyboard Hook and synthesizes no input.

The modeless native window contains:

- a category tree on the left;
- a checkbox list on the right;
- a status line;
- `Refresh`, `Apply`, and `Close` buttons.

Categories are:

1. Campaigns
   - Add-on: Trojan, Roman, Mayan, Viking, Settlement
   - Mission CD: Bonus, Roman, Viking, Mayan, Conflict/Settlement
   - New World
   - New World 2
   - Original Campaigns
   - Dark Tribe
2. Fixed maps
   - Single Player
   - Multiplayer-list maps playable from Single Player
   - Custom maps
3. Historical or currently unavailable records
4. Random-map records (read-only, marker-hidden)

Campaign rows use the immutable 107-row descriptor catalog. Fixed-map rows are
discovered by bounded, non-recursive enumeration of the exact admitted map
directories below the game root and are identified by normalized relative
`.map` paths. Display labels are auxiliary only.

Random maps remain governed by exact confirmed `identity.relative`. They may
remain persisted for history, but the manager labels them marker-hidden and
does not allow a checkbox operation to convert them into fixed/campaign
markers.

Checkbox changes remain in the window until `Apply`. Closing or pressing
`Close` discards unapplied changes. Successful apply refreshes both marker
indexes immediately.

## Catalog model

The UI consumes an immutable `CompletionManagerCatalog` snapshot. Each entry
contains:

- stable ID;
- exact relative ID;
- auxiliary display label;
- category ID;
- canonical map kind and launch source;
- current checked state;
- editable/read-only state;
- availability state.

Catalog construction rules:

- campaign entries come only from an admitted exact-version descriptor;
- fixed entries come only from validated `.map` files under one of the three
  approved roots;
- existing database records are joined by exact stable ID;
- an existing non-random record absent from the current installation appears
  under historical/unavailable and may be unchecked;
- random records appear read-only;
- case-fold or normalization collisions make every colliding entry read-only
  and emit an ambiguity status;
- visible text, save names, `identity.name`, and arbitrary `RD` strings never
  create or reclassify an entry.

The filesystem enumerator has fixed limits for path length, entry count, and
total UTF-16 units. Reparse points, directories, non-`.map` extensions,
control characters, traversal, recursive enumeration, and overflow fail
closed.

## Manual records

A newly checked entry creates a normal `CompletionRecord` from the catalog's
exact relative identity:

- stable ID from `BuildStableMapId`;
- map kind from `CompletionKindFor`;
- canonical campaign/fixed launch source from the catalog;
- auxiliary display name from the descriptor label or file stem;
- UTC application time;
- record source `manual-manager`;
- current supported game/plugin persistence versions.

JSON validation is extended to admit exactly the historical
`native-event-609` source and the new `manual-manager` source. No other record
source is accepted.

Unchecking removes the exact stable ID only. It never deletes a map, save,
campaign-progress value, log, backup, or unrelated JSON field.

## Conflict-safe atomic apply

`CompletionStore` gains a monotonic in-process revision. `SnapshotWithRevision`
returns an owned snapshot and its revision.

The manager submits:

- the baseline revision captured by `Refresh`;
- a bounded set of desired editable stable IDs;
- exact catalog entries for newly checked IDs.

The worker serializes manual commands with automatic victory candidates. The
store rejects the command without file I/O when:

- the baseline revision is stale;
- the store is not writable;
- an ID is unknown, ambiguous, random/read-only, or duplicated;
- a new record fails exact validation;
- capacity is exceeded.

For an accepted command the store derives one candidate snapshot, encodes it,
writes and flushes the temporary file, rereads and validates it, backs up the
current main, and atomically replaces the main. Only after `Committed` does it
publish the new snapshot and increment the revision.

If a victory commits while the window is open, the revision changes. `Apply`
then reports a conflict and requires `Refresh`; it never overwrites the
victory.

No-op apply performs no file I/O and returns `Unchanged`.

## Window and thread ownership

The manager owns one UI thread and at most one top-level window. The game
callback never creates controls or waits for the UI:

1. the main-menu tick edge-detects `Ctrl+Shift+M`;
2. it posts an open request to the manager thread;
3. the manager requests an owned catalog/snapshot from the runtime;
4. checkbox work stays on the manager thread;
5. `Apply` disables mutation controls and invokes one bounded store command on
   the manager thread;
6. the store mutex serializes that command with the automatic-victory worker,
   and the result is rendered before controls are re-enabled.

The database is small and bounded, so Phase 7A accepts the brief manager-window
pause during its verified temporary-write/flush/replace sequence. The game
render and listener callbacks never wait for it. The manager never reads game
memory. Map-directory enumeration and database snapshot copying occur off the
render callback. Only immutable descriptor data and owned strings cross
threads.

While the manager is open, leaving the main menu disables `Apply`. The window
may remain visible for inspection but cannot mutate state until a fresh main
menu `Refresh`.

## Read-only and recovery states

`ReadOnlyBackup`, `ReadOnlyNormalizationFailed`, and
`ReadOnlyUnavailable` are displayed prominently. Checkboxes and `Apply` are
disabled.

Phase 7A does not automatically repair a database. A later Phase 7B recovery
button may be enabled only after a separate design proves:

- the exact backup is valid;
- the corrupt/unavailable main is preserved as evidence;
- recovery uses a new temporary file and atomic replacement;
- recovery is an explicit two-step user action;
- failure never overwrites the only valid copy.

## Configuration

The packaged INI documents the fixed Phase 7A manager policy:

```ini
Enabled=1
OpenHotkey=Ctrl+Shift+M
ClassifiedCampaigns=1
ClassifiedFixedMaps=1
ManualAtomicApply=1
RandomRecordsReadOnly=1
RevisionConflictProtection=1
```

These values are package assertions rather than mutable runtime settings.
Phase 7A intentionally supports only the fixed shortcut and safety policy;
configuration parsing or hot reload is deferred until it can be fail-closed
without adding per-frame disk reads.

## Shutdown

Controlled shutdown:

1. disables new open/apply requests;
2. closes the manager window;
3. joins the UI thread within a bounded timeout;
4. disables completion admissions;
5. detaches native subscription and removes public listeners;
6. drains the serialized worker;
7. releases store and rendering resources.

If the UI thread or worker does not stop within its timeout, resources are
retained and the failure is logged. The plugin never terminates the game or UI
thread forcibly.

## RED/GREEN contract

Constructed tests must prove:

- all 107 admitted campaign descriptors appear once in the correct family;
- fixed-map enumeration accepts only exact approved roots and `.map` files;
- normalization collisions, traversal, reparse points, overflow, and random
  identities fail closed;
- existing records join by stable ID, not display name;
- `RD_PlayerSave` auxiliary text cannot affect classification;
- manual check adds one `manual-manager` record;
- manual uncheck removes only the exact stable ID;
- random records are read-only;
- no-op and stale-revision commands perform zero file I/O;
- automatic victory between refresh/apply causes conflict, not lost update;
- committed manual edits publish both marker indexes once;
- duplicate, read-only, invalid, and failed transactions do not publish;
- window open requests are edge-triggered and main-menu gated;
- shutdown order is bounded;
- all existing persistence, RD, campaign, renderer, and mutation tests remain
  GREEN.

Intermediate RED remains local. Only an isolated complete GREEN checkpoint may
be pushed. Deployment and live database use require fresh explicit approval
after CI and artifact audit.

## Completion boundary

Phase 7A GO means the audited candidate can display a complete classified
catalog and safely apply checkbox changes through one conflict-aware atomic
transaction. It does not authorize database recovery, deployment, or a live
manual edit.
