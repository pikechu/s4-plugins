# Phase 5C database read-only forensics report

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The primary completion database is valid JSON containing three internally
consistent completion records. Its `InvalidRecord` result is caused by a
reader-compatibility regression, not by malformed JSON, unsafe map identity,
RD-name classification, or an untrustworthy completion event.

The recommended recovery is a code-only compatibility correction that admits
the known persistence version `0.5.0`. The primary database must not be edited,
replaced from the stale backup, normalized, or automatically repaired.

This report is read-only evidence. It authorizes no database or deployment
write.

## Frozen evidence

Live project data was read and copied while Settlers United processes were
present. No game or project data file was opened for write, renamed, deleted,
or replaced, and no process was started or terminated.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `completed_maps.json` | 951 | `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |
| `completed_maps.json.bak` | 639 | `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54` |

Exact read-only copies are retained below the ignored project artifact tree:

`artifacts/phase-5c-database-read-only-forensics/2026-07-16/`

Their hashes match the live files exactly. Neither the production temporary
path nor a deployment temporary sibling existed.

## Record comparison

The primary database contains three records, in stable-ID order:

1. `Map\Singleplayer\Aeneas.map`, plugin `0.4.0`;
2. `Map\Singleplayer\TheCross.map`, plugin `0.5.0`;
3. `Map\User\Antares.map`, plugin `0.4.0`.

The backup contains Aeneas and Antares only. Those two record objects are
field-for-field identical between primary and backup. Restoring the backup as
the primary database would therefore discard the legitimate TheCross
completion and is not an acceptable recovery.

The main database has exactly one `plugin_version:"0.5.0"` field. It begins at
byte offset 623 within the compact UTF-8 JSON. No other record uses that
version.

## Exact production rejection

The current production decoder accepts a record version only when:

```cpp
version == "0.4.0" || version == kCompletionPluginVersion
```

The current `kCompletionPluginVersion` is `0.6.0`, so the known `0.5.0`
record is rejected with `CompletionJsonFailure::InvalidRecord` (enumerator
value 10). The parser then loads the valid two-record backup in
`ReadOnlyBackup` mode and disables future writes.

All other TheCross fields satisfy the current record contract:

- stable ID: `map:map\singleplayer\thecross.map`;
- relative identity: `Map\Singleplayer\TheCross.map`;
- display name: `TheCross`;
- kind: `fixed`;
- launch source: `single-player-map`;
- completion UTC: `2026-07-14T11:13:02Z`;
- record source: `native-event-609`;
- game version: `2.50.1516.0`.

The stable ID is the normalized relative identity, the relative identity is a
fixed Single Player map path, the map kind and launch source agree, and the UTC
timestamp is canonical.

## Provenance of the `0.5.0` record

Version `0.5.0` was the reviewed Phase 5A fixed-map row-calibration build, not
an unknown or pre-persistence version. The Phase 5A report explicitly records
that schema version 1 admitted persistence versions `0.4.0` and current
`0.5.0`, while rejecting older or unknown versions.

The project log records the exact deployed `0.5.0` runtime and candidate ASI,
then the accepted and committed TheCross victory:

```text
2026-07-14T19:06:21.857+LOCAL [INFO] CampaignCompletionDebug bootstrap version=0.5.0 ...
2026-07-14T19:13:02.174+LOCAL [INFO] completion-admission accepted stable-id=map:map\singleplayer\thecross.map
2026-07-14T19:13:02.184+LOCAL [INFO] completion-store committed stable-id=map:map\singleplayer\thecross.map stage=none error=0
```

The stored UTC time `11:13:02Z` is the UTC representation of that local commit
time. Commit `9fb655d` later changed `kCompletionPluginVersion` from `0.5.0` to
`0.6.0` without adding `0.5.0` to the historical allowlist. That version bump
made the previously valid record unreadable.

## RD-name boundary

The TheCross determination uses the confirmed relative identity
`Map\Singleplayer\TheCross.map`. It does not use a save filename or display
name and has no `RD_`, square-bracket, or angle-bracket random identifier.
Nothing in this finding changes the established rule that random-map
classification uses only confirmed, session-bound `identity.relative`.

## Recovery conclusion

The primary database is the authoritative superset and should remain
byte-for-byte preserved. A parser compatibility correction can restore
`WritableLoaded` mode with all three records and no startup transaction,
because their stored launch sources are already canonical.

Editing the historical `plugin_version`, copying the two-record backup over the
main file, accepting arbitrary semantic versions, or enabling automatic repair
would either falsify provenance, lose a valid completion, or weaken the schema
boundary. All four approaches are rejected.
