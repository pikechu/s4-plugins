# Completion Persistence Design

## 1. Scope

Version `0.4.0` adds automatic persistence of confirmed local-player
victories. It does not render completion markers or add manual completion
controls.

The feature consumes the already validated chain of Settlers United map
identity, session origin policy, and native event `609` (`0x261`). It writes a
record only when all three observations belong to the same nonzero MapInit
session, the native result is `won`, the map identity is confirmed, and
`ShouldRecordVictory` accepts the refined origin.

Random-map victories remain recordable. Their records have `map_kind=random`,
and the later marker phase must continue to suppress them through
`ShouldShowCompletionMarker`.

## 2. Directory migration and path ownership

The ASI is loaded from:

```text
<game>\Plugins\CampaignCompletionDebug.asi
```

It derives its private directory from the ASI module path, never from the
process current directory:

```text
<game>\Plugins\CampaignCompletion\
```

This directory owns:

- `CampaignCompletionDebug.ini`
- `CampaignCompletion.log`
- `completed_maps.json`
- `completed_maps.json.tmp`
- `completed_maps.json.bak`
- the existing controlled-stop file when diagnostic control remains enabled

On 2026-07-14 the project-owned directory was moved from `<game>\CampaignCompletion`
to `<game>\Plugins\CampaignCompletion` after confirming `S4_Main.exe` was
closed. Both files were copied first to
`research/backups/campaign-completion/2026-07-14-pre-plugins-migration/` and
verified by SHA-256 before and after the move. The old directory no longer
exists.

The deployed `0.3.3` binary still resolves the old location. The game must not
be started until a path-aware build is deployed; otherwise that binary could
recreate the old directory and miss the migrated INI.

No file outside the project-owned `CampaignCompletion` directory and the
separately authorized `CampaignCompletion*.asi`/guarded Settlers United
archive workflow may be created, modified, moved, or deleted.

## 3. Components

### 3.1 CompletionCandidateCoordinator

This game-thread component owns one bounded state record for the current
MapInit session. It accepts:

- a new session and its initial `LaunchOriginSnapshot`;
- a confirmed `ConfirmedMapIdentity` and refined origin for that session;
- a consumed `VictoryEventSnapshot` for that session.

It emits at most one immutable `CompletionCandidate` per session. Emission
requires:

```text
session_id != 0
AND identity.session_id == session_id
AND victory.session_id == session_id
AND victory.result == Won
AND identity is confirmed
AND ShouldRecordVictory(refined_origin)
```

Identity and victory may arrive in either order. A new MapInit discards all
unemitted state from the preceding session. A lost, malformed, conflicting,
or orphan event cannot produce a candidate. Once emitted, repeated native
callbacks or identity observations cannot emit again.

The coordinator performs no disk I/O and never carries observations across
sessions.

### 3.2 CompletionStore

`CompletionStore` owns the validated in-memory snapshot, stable-ID index,
JSON codec, and transaction state. It exposes load, query, and add-if-absent
operations. Records are immutable after insertion. An existing stable ID is a
successful no-op: the first completion time and original metadata remain
unchanged, and no file transaction occurs.

The store separates schema logic from operating-system file operations. A
small injected atomic-file interface permits deterministic failure testing;
the production implementation uses Win32 wide-path file APIs.

### 3.3 CompletionWorker

`CompletionWorker` owns one background thread and a queue with capacity `32` of
immutable candidates. The game thread only attempts a nonblocking enqueue.
Queue exhaustion rejects the candidate and writes an error; it never waits
inside an S4ModApi or native-event callback.

The worker serializes store operations. Normal controlled shutdown stops new
admissions and drains pending work within a bounded wait. Unexpected process
termination can leave only a temporary file; it cannot turn that file into an
authoritative database.

## 4. Runtime data flow

```text
MapInit
  -> create session and capture initial origin
SU identity confirmation
  -> refine that session's origin and retain identity
native event 609 with local result won
  -> retain victory for that session
session gate satisfied
  -> enqueue immutable completion candidate once
worker
  -> add if stable ID absent
  -> validate and atomically commit JSON snapshot
```

The same flow handles direct launches and loaded saves. A loaded fixed map and
the same directly launched fixed map resolve to one stable ID. A loaded-save
path is never persisted.

## 5. Stable identity and record schema

`completed_maps.json` is UTF-8 and uses schema version `1`:

```json
{
  "schema_version": 1,
  "records": [
    {
      "stable_id": "map:map\\user\\battle of the gods.map",
      "relative_id": "Map\\User\\Battle of the Gods.map",
      "display_name": "Battle of the Gods",
      "map_kind": "fixed",
      "launch_source": "load-single-player-map",
      "completed_at_utc": "2026-07-14T01:57:38Z",
      "record_source": "native-event-609",
      "game_version": "2.50.1516.0",
      "plugin_version": "0.4.0"
    }
  ]
}
```

Every record contains exactly the required fields shown above; unknown
top-level or record fields are rejected when reading schema version `1`.
Version `1` does not emit empty campaign or mission fields; a later campaign
adapter must
change the schema deliberately if it needs new persisted fields.

### 5.1 Stable-ID rules

- Start only from a validated SU relative identifier.
- Convert path separators to backslashes.
- Apply invariant Unicode lowercase mapping for case-insensitive matching.
- Prefix the canonical value with `map:`.
- Reject empty values, absolute paths, parent traversal, invalid UTF-16,
  embedded NULs, or values longer than `512` UTF-16 code units, matching the
  existing SU identity admission bound.
- Reject the generated stable ID if invariant lowercase mapping makes it
  longer than `1024` UTF-16 code units.
- Never include a game, save, profile, or user absolute path.

The original validated relative identifier and display name are retained in
`relative_id` and `display_name` for diagnosis and future UI matching. They do
not affect uniqueness.

`map_kind` is `random` exactly when the approved random-map identifier policy
classifies the relative identifier as random; otherwise it is `fixed`.
`launch_source` records the refined source of the first accepted victory but
does not participate in deduplication.

`completed_at_utc` is an ISO 8601 UTC timestamp with whole-second precision.
It is captured on the game thread when the complete session gate first
accepts the victory, not later when the worker reaches the queue entry.
`record_source` is exactly `native-event-609` in this phase.

## 6. Load modes

The store enters one of these modes once at startup:

1. **WritableEmpty**: neither the main file nor a backup exists. The first
   accepted record may create the main file. A stale temporary file does not
   prevent this mode and may be overwritten only by a new transaction.
2. **WritableLoaded**: the main file parses and passes every schema invariant.
3. **ReadOnlyBackup**: the main file is missing or invalid while the backup is
   valid. Queries use the backup snapshot, all writes are refused for the rest
   of the process, and the log instructs the user to repair or restore the
   files manually.
4. **ReadOnlyUnavailable**: at least one main or backup file exists, the main
   file is missing or invalid, and no valid backup is available. No completion
   records are exposed, all writes are refused, and existing files remain
   untouched.

A file is also invalid when it exceeds `4 MiB`, contains more than `8192`
records, or contains any string field longer than `1024` UTF-16 code units
after strict UTF-8 decoding.

A stale `.tmp` is never loaded, promoted, or automatically deleted. A schema
version other than `1`, malformed JSON, invalid UTF-8, duplicate object keys,
unknown fields, missing or wrongly typed required fields, invalid enum strings,
invalid UTC time, invalid stable ID, duplicate stable IDs, or an input over the
fixed size limit makes the file invalid.

## 7. Transaction protocol

For a new stable ID, the worker:

1. Builds a copy of the current snapshot containing the new immutable record.
2. Sorts records deterministically by `stable_id` and serializes the complete
   schema to `completed_maps.json.tmp`.
3. Flushes the temporary file with Win32 file-buffer flushing.
4. Reopens the temporary file and runs the same bounded parser and all schema
   invariants used at startup.
5. If the main file exists, uses a Win32 atomic replacement that also writes
   `completed_maps.json.bak`.
6. If this is the first file, atomically moves the temporary file into place
   with write-through semantics.
7. Publishes the new in-memory snapshot only after the commit succeeds.

Failure at any step preserves the preceding main file and preceding published
snapshot. Errors are reported with the stable ID, transaction stage, and
Win32 error code. Logs do not include absolute save or user paths.

## 8. Configuration and lifecycle

Version `0.4.0` changes the diagnostic configuration to:

```ini
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=0
```

The runtime opens the migrated INI and log relative to the ASI module
directory. Startup loads the completion store before listener admission can
produce candidates, then starts the worker. Shutdown ordering is:

1. reject new completion candidates;
2. detach the native subscriber and stop public listeners using the existing
   safe order;
3. drain the completion queue for at most `5000 ms`;
4. close trace, store, worker, and log resources.

If the queue does not drain within `5000 ms`, controlled stop reports failure
and retains the worker, store, and logger instead of destroying resources that
the worker could still access. Process termination may then end the retained
thread under normal operating-system process teardown.

File, parser, encoding, clock, allocation, and queue exceptions are contained
inside the persistence boundary and never propagate into game callbacks.

## 9. Automated tests

### 9.1 Candidate coordination

Tests cover identity-first and victory-first arrival, exact session matching,
new-session reset, duplicate callbacks, lost/malformed/conflict/orphan events,
unknown identity, unknown eligibility, online multiplayer rejection, fixed
map admission, loaded fixed-map admission, and random-map admission.

### 9.2 Data and codec

Tests cover separator and case normalization, direct/load stable-ID equality,
random classification, invariant Unicode case mapping, invalid path rejection,
UTF-8 escaping, deterministic ordering, UTC formatting, normal JSON
round-trips, truncation, illegal UTF-8, duplicate keys and IDs, bad schema,
missing/wrong fields, invalid enums/timestamps, and oversized input.

### 9.3 Transaction failure injection

An injected fake file boundary fails each of temporary creation, write, flush,
reopen, validation, backup, replace, and first-create move. Every failure must
leave the old published snapshot and old main bytes unchanged. Additional
tests prove valid-main writable load, missing-main first creation, corrupt-main
valid-backup read-only load, corrupt-main invalid-backup unavailability, stale
temporary-file nonpromotion, duplicate victory no-op, bounded queue rejection,
and bounded worker shutdown.

All destructive persistence tests run in project-owned temporary directories,
never against the live game data directory.

## 10. Deployment and live validation

Deployment requires the game to be closed. Before changing the authorized
Settlers United archive or project-owned plugin files, preserve and verify the
existing immutable SU archive backup and create a separate verified backup of
the migrated `Plugins\CampaignCompletion` directory.

Live gates for `0.4.0` are:

1. The new build reads the migrated INI and writes its log below
   `Plugins\CampaignCompletion`; the old root-level directory is not recreated.
2. A fresh fixed Custom Maps victory obtained through the approved AI-resign
   test creates one `fixed` record.
3. Repeating that victory leaves the JSON bytes and first-completion time
   unchanged.
4. A fresh random-map victory creates one `random` record.
5. A voluntary exit and a loss add no record.
6. When a practical near-victory fixed save is available, a manual loaded-save
   victory resolves to the same stable ID and creates no duplicate record.
7. The game remains responsive after each sample and during normal exit.

Any cross-session association, malformed write, main-file damage, callback
exception, queue deadlock, old-directory recreation, or game responsiveness
regression is a NO-GO. UI marker development does not begin until these gates
pass or any explicitly deferred gate is documented with its reason.
