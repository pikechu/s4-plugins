# Phase 5C database compatibility recovery design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Restore writable completion persistence without modifying the existing
three-record primary database. Then complete the deferred same-process victory
and immediate-marker-refresh acceptance.

This design does not itself authorize implementation, deployment, database
replacement, or a live victory write.

## Compatibility correction

Replace the moving two-value version rule with an explicit, reviewed
persistence-version allowlist:

- `0.4.0`: initial completion persistence;
- `0.5.0`: Phase 5A row-calibration build that legitimately wrote TheCross;
- the current persistence writer version, presently `0.6.0`.

The comparison remains exact and ordinal. It must not accept version ranges,
prefixes, later unknown versions, pre-persistence versions, malformed values,
or the diagnostic bootstrap/INI version merely because it is newer.

Stored `plugin_version` values are immutable provenance. Loading or encoding a
snapshot must preserve `0.4.0` and `0.5.0` record values exactly; only newly
committed records receive the current writer version.

## Required tests

Test-first implementation must prove:

1. the exact live three-record primary JSON decodes successfully;
2. `0.4.0`, `0.5.0`, and current `0.6.0` records are accepted;
3. `0.3.3`, `0.5.1`, `0.6.1`, an empty value, and arbitrary versions remain
   `InvalidRecord`;
4. decode/encode preserves every historical record field, including
   `plugin_version`;
5. a main database containing the three records loads as `WritableLoaded`,
   publishes all three stable IDs, and performs zero writes or replacements;
6. the existing invalid-main/valid-backup path remains read-only for genuinely
   malformed or unknown records;
7. duplicate, random-map, online exclusion, RD-prefixed save-name, atomic
   replacement, and failure-injection tests remain unchanged and GREEN.

The live database sample remains evidence outside version control. A minimal
fixture with the same fields may be committed; tests must not read a developer
or game-directory path.

## Startup recovery acceptance

After complete Windows CI and artifact audit, deployment requires fresh user
approval and closed game/SU processes. Only the audited project ASI and INI may
be deployed through the existing guarded archive procedure.

The database and backup are preconditions, not deployment targets:

- main SHA-256 must be
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`;
- backup SHA-256 must be
  `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`.

On the first startup, require:

- the audited ASI hash and compatible executable admission;
- `completion-store mode=writable-loaded records=3 failure=0 error=0`;
- all three stable IDs, including TheCross, in the published marker index;
- TheCross marked in Single Player Maps when visible;
- byte-identical main and backup hashes before startup and after normal exit;
- no database temporary file and no normalization or commit log.

Any startup write, loss of TheCross, backup fallback, unexpected marker, or
database hash drift is NO-GO.

## Same-process commit and immediate refresh

Only after startup recovery is GO may a separately approved live write test
begin. In a fresh process:

1. freeze the three-record main and two-record backup hashes and counts;
2. select one eligible, previously uncompleted fixed map by confirmed relative
   identity, never by its save/display name;
3. obtain one real local-player native victory event `609` through the already
   accepted user-driven flow;
4. require one `CompletionAddStatus::Committed` result for the new stable ID;
5. return to the correct fixed-map list without restarting the process;
6. require exactly one marker on the new row on the first admitted frame;
7. repeat the victory or replay the observation and require duplicate/no-write
   behavior;
8. exit normally and audit all files.

The successful transaction must produce a four-record primary database. Under
the existing `ReplaceFileW` contract, the backup becomes the exact three-record
pre-commit primary. The old two-record backup hash is therefore expected to
change only at this explicitly approved commit boundary. No other file or
record may change.

## Failure and rollback

- A compatibility build that still loads the backup is not allowed to write.
- A startup normalization attempt is a failure even if its output is valid.
- A failed live transaction must retain the three-record main snapshot and
  must not publish a marker for the candidate.
- The immutable read-only evidence copies and pre-deployment hashes remain the
  recovery anchors.
- Do not replace the main database with the two-record backup.
- Do not rewrite `0.5.0` as `0.6.0`.
- Do not broaden version acceptance to make a failing fixture pass.
- Any database restoration or manual file replacement requires a new, exact
  user approval separate from ASI deployment approval.

## Completion boundary

Phase 5C is GO only when the code-only compatibility fix has a full Windows CI
and audited deployment, startup restores writable three-record state with zero
database writes, and the separately approved same-process victory adds exactly
one record whose marker appears without restart. Until then, Phase 5B marker
display remains GO but writable persistence and end-to-end release readiness
remain incomplete.
