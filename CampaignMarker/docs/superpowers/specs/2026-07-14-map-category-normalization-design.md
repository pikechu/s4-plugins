# Map Category Normalization Design

## 1. Purpose

Completion storage must represent the same map identically whether the player
starts it from a fixed-map list or loads a save. Navigation is reliable for
deciding whether a session is an excluded online-multiplayer session, but it is
not a stable map-category identity: Settlers IV draws sibling selector pages in
one frame, and loading a save bypasses the original fixed-map list.

Settlers United supplies the validated map name and relative map path after
map initialization through `SU.Game.GetMapName()` and
`SU.Game.GetMapNameRelativePath()`. Live evidence has already confirmed that a
loaded Custom Map resolves to `Map\User\Battle of the Gods.map`, while direct
launches resolve Aeneas to `Map\Singleplayer\Aeneas.map` and Antares to
`Map\User\Antares.map`. The game installation separates fixed maps into
`Map\Singleplayer`, `Map\Multiplayer`, and `Map\User`.

The plugin will therefore separate two decisions:

- the observed navigation/load context determines whether recording is
  allowed; and
- the confirmed SU relative identity determines the category persisted for an
  eligible map.

No save parser, new game hook, process-memory probe, or game-directory write is
introduced.

## 2. Canonical categories

After SU identity validation, an eligible session receives one canonical
completion source:

| Validated identity or session class | Canonical `launch_source` |
| --- | --- |
| Random-map identifier | `random-map` |
| `Map\Singleplayer\...` | `single-player-map` |
| `Map\Multiplayer\...` | `single-player-multiplayer-map` |
| `Map\User\...` | `single-player-custom-map` |
| Direct or loaded campaign session | `campaign` |
| Other validated fixed-map relative path | `single-player-map` |

Prefix comparison is case-insensitive, accepts either slash form after the
existing relative-path validator has rejected absolute paths and traversal,
and requires a complete directory component. For example,
`Map\Userland\X.map` does not match `Map\User\...`.

The generic fixed-map fallback implements the agreed failure mode: when a map
is identifiable but does not belong to one of the three known fixed-map
directories, the plugin records it without inventing a finer category.

`launch_source` is retained for schema-version-1 compatibility, but for all new
writes its value describes the canonical map category, not whether the current
session was entered through the load menu.

## 3. Admission and data flow

The existing load controls and navigation tracker remain responsible for
admission:

- an online-multiplayer menu or multiplayer-save selection remains excluded;
- a campaign or campaign-save selection remains eligible;
- an ordinary single-player save begins unresolved and becomes eligible only
  after a validated SU identity is associated with the active map session; and
- a random map remains recordable but its future completion marker remains
  hidden.

Online exclusion is applied before identity classification. Consequently, an
online session using a map stored under `Map\Multiplayer` cannot become
eligible merely because its relative path resembles an offline fixed map.

For an eligible session the data flow is:

```text
navigation/load controls
  -> admission context (eligible, excluded, or unresolved)
MapInit + SU identity confirmation
  -> stable map ID and canonical map category
native event 609: local player won
  -> immutable completion candidate
completion store
  -> add once by stable ID and atomically commit
```

Direct and loaded instances of the same map therefore produce the same
`stable_id`, `relative_id`, `map_kind`, and `launch_source`. The completion
timestamp still records the first accepted victory only.

## 4. Existing schema-version-1 records

The deployed database may contain route-based or incorrectly downgraded source
values, including the current Antares record stored as `single-player-map`.
After a normal writable load, the store will canonicalize each accepted record
from its already validated `relative_id` and its campaign/random semantics.

Normalization may change only `launch_source`. It must preserve:

- record count and ordering;
- `stable_id`, `relative_id`, and display name;
- map kind;
- first completion timestamp;
- record source; and
- game and plugin versions.

If no record changes, startup performs no database write. If at least one
record changes, the store validates the full normalized snapshot and commits it
once through the existing temp-file, flush, read-back, `ReplaceFileW`/backup
transaction. Read-only recovery mode never normalizes on disk and logs that
manual handling is still required.

Legacy schema-version-1 source values remain readable so an existing valid
database can be normalized without a schema bump. New commits emit only the
canonical values above.

## 5. Component boundaries

### Map-category classifier

A small pure function accepts a validated relative identity plus the admitted
session class and returns the canonical completion source. It has no I/O and
does not decide online eligibility.

### Session policy

The existing session policy calls the classifier only after matching the SU
identity to the active session. It continues to fail closed for unresolved
ordinary saves and excluded online sessions.

### Completion store

The store applies the same classifier while loading legacy records. It writes
only when normalization changes a record and uses the existing bounded,
validated atomic transaction.

The UI will continue to use stable map identity for marker lookup. Category is
metadata and a future list filter; it never replaces the stable ID.

## 6. Error handling

- Missing, invalid, conflicting, or stale SU identity leaves an ordinary loaded
  map unresolved and prevents a victory record.
- Invalid or unsafe relative paths are rejected by the existing validator
  before classification.
- Unknown but valid fixed-map paths use the generic `single-player-map`
  category.
- Online-multiplayer exclusion always wins over path classification.
- A normalization transaction failure follows the existing store failure
  policy and must not publish a partially rewritten snapshot.
- A corrupt main database with a valid backup remains read-only; this feature
  must not turn recovery into an automatic write.

## 7. Verification

Tests will be written before implementation and will cover:

1. direct and loaded `Map\Singleplayer` identities produce
   `single-player-map`;
2. direct and loaded `Map\Multiplayer` identities produce
   `single-player-multiplayer-map` when the session is offline;
3. direct and loaded `Map\User` identities produce
   `single-player-custom-map`;
4. an online session remains excluded for every relative-path category;
5. random-map recording and marker hiding remain unchanged;
6. campaign and loaded-campaign records both persist as `campaign`;
7. a valid unknown fixed-map path falls back to `single-player-map`;
8. a legacy or downgraded record is normalized without changing its stable ID
   or first completion time;
9. an already canonical database causes no startup write;
10. read-only recovery mode performs no normalization write; and
11. the existing full test, build, packaging, PE32, protected-entry, and
    mutation-policy checks still pass.

Live validation will use a Custom Map different from an already completed map,
or the normalized existing Antares record, and will compare the persisted
category with the SU relative identity. Deployment remains blocked while the
game process is running and requires the existing guarded archive procedure
after the user closes the game.
