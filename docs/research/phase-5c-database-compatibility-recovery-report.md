# Phase 5C database compatibility recovery report

Date: 2026-07-16 (Asia/Shanghai)

## Decision

Phase 5C is **GO**. The audited code-only compatibility correction loaded the
untouched three-record primary database in writable mode, published the
historical TheCross completion, and performed no startup write. A separately
approved same-process Phoenix victory then added exactly one fourth record,
atomically retained the three-record pre-commit primary as the backup, and
published the Phoenix marker without a process restart.

This report authorizes no further live database write, restoration, manual
database replacement, or deployment.

## Audited candidate

- implementation commit: `3a5ea119b03734838da5cbb42fc975495afba216`
- Windows CI run: `29472314013` (GREEN)
- downloaded outer artifact SHA-256:
  `ea02b7d884f1beb8c8705c1d81200ba2a77b146189682f3cfb55841c323846c7`
- inner candidate package SHA-256:
  `7d03ddfdeb7cb1a2471105f09c734aebcc28ee4e9f7ca7d8291e45992aeb12ed`
- installed `Plugin_SU.zip` SHA-256:
  `14134014ec61ad9627e3cff1e4945fce7d04d2db934a3d47fc70f63731c614ca`
- installed embedded ASI SHA-256:
  `bf37065c1e175f42f4f896daa6ae2eb29ae8747c874008a65fd37cd6fec2cca6`
- installed INI SHA-256:
  `882e58a9b4c5ae91045175af5a2623430e222bf2d264ac73f52305d02a4d61cb`

The live log identifies runtime `0.6.3` in
`database-compatibility-recovery` mode, the same audited ASI hash, a compatible
executable, and an admitted read-only internal-menu adapter.

## Startup recovery result

The accepted game process logged:

```text
2026-07-16T13:26:54.932+LOCAL [INFO] CampaignCompletionDebug bootstrap version=0.6.3 pid=33524 mode=database-compatibility-recovery
2026-07-16T13:26:55.162+LOCAL [INFO] module name=CampaignCompletionDebug.asi ... sha256=bf37065c1e175f42f4f896daa6ae2eb29ae8747c874008a65fd37cd6fec2cca6
2026-07-16T13:26:55.162+LOCAL [INFO] executable compatibility=compatible
2026-07-16T13:26:55.162+LOCAL [INFO] internal-menu-adapter=admitted-read-only
2026-07-16T13:26:55.609+LOCAL [INFO] completion-store mode=writable-loaded records=3 failure=0 error=0
```

The user then entered Single Player Maps, scrolled to TheCross, and confirmed
that its completion marker was visible. The internal menu supplied the exact
confirmed relative identifier `Map\Singleplayer\TheCross.map` in 13 captured
row snapshots across scroll positions 33 through 37. The finding therefore
uses `identity.relative`; it does not classify from the display name or a save
name and does not change the RD-name boundary.

Within the accepted session there was no `completion-store committed` record,
normalization, database write/replacement record, read-only backup fallback, or
load failure. The user returned and closed the game and Settlers United
normally. The final protected-process audit reported zero instances of
`S4_Main`, `Settlers United`, or `SettlersUnited`.

## Zero-write file audit

The primary and backup remained byte-identical to the frozen pre-startup
evidence:

| File | Bytes | Last write (local) | SHA-256 |
| --- | ---: | --- | --- |
| `completed_maps.json` | 951 | `2026-07-14 19:13:02.1756072` | `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |
| `completed_maps.json.bak` | 639 | `2026-07-14 12:43:36.8617886` | `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54` |

The primary still contains exactly Aeneas `0.4.0`, TheCross `0.5.0`, and
Antares `0.4.0`; the backup still contains exactly Aeneas and Antares. The
production database temporary path and the checked sibling temporary paths
were absent after exit.

## Same-process commit and immediate refresh

The user separately approved one live completion write. The preflight froze the
same three-record primary and two-record backup used by startup recovery, then
started a fresh process. PID `30748` loaded the audited ASI and reported
`completion-store mode=writable-loaded records=3 failure=0 error=0` before any
map launch.

The user selected Phoenix directly from Single Player Maps and used the
previously accepted automatic AI Resign victory flow. The session confirmed
the exact relative identity before admission:

```text
2026-07-16T13:41:31.057+LOCAL [INFO] su-map-relative=Map\Singleplayer\Phoenix.map
2026-07-16T13:41:31.057+LOCAL [INFO] identity-association=confirmed
2026-07-16T13:41:41.347+LOCAL [INFO] completion-admission accepted stable-id=map:map\singleplayer\phoenix.map
2026-07-16T13:41:41.363+LOCAL [INFO] completion-store committed stable-id=map:map\singleplayer\phoenix.map stage=none error=0
```

The new record is exactly:

- stable ID: `map:map\singleplayer\phoenix.map`;
- relative identity: `Map\Singleplayer\Phoenix.map`;
- map kind: `fixed`;
- launch source: `single-player-map`;
- completion UTC: `2026-07-16T05:41:41Z`;
- record source: `native-event-609`;
- plugin persistence version: `0.6.0`.

This admission used the confirmed session-bound relative identity, not the
display name or any save name. The established RD archive-name rule therefore
remains unchanged.

The main database grew from 951 bytes and three records to 1,260 bytes and four
records with SHA-256
`49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e`.
The backup became byte-for-byte equal to the exact frozen three-record
pre-commit primary, with SHA-256
`31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`.
All Aeneas, TheCross, and Antares fields and historical plugin versions were
preserved.

Without restarting PID `30748`, the user returned to Single Player Maps. The
first captured visible row set contained `Map\Singleplayer\Phoenix.map`, and
the user confirmed its marker appeared automatically without hover. The user
then scrolled Phoenix out of view and back; the marker again appeared
immediately. The complete session contained one bootstrap, exactly one store
commit, exactly one Phoenix commit, and no warning, error, second write, or
temporary database sibling.

After normal exit, the protected-process count was zero. The four-record main
and three-record backup retained their post-commit hashes and timestamps, and
the backup still compared byte-for-byte equal to the frozen pre-commit main.

## Frozen live evidence

The complete project log after normal exit is copied beneath the ignored
artifact tree at:

`artifacts/phase-5c-database-compatibility-recovery/run-29472314013/live-startup-recovery/CampaignCompletion.log`

It is 12,467,773 bytes with SHA-256
`0f5a1aa324a608838209ed896e32abe5e7d8dd6f23cd483719e08c1946f69855`.
The candidate audit and guarded deployment result remain beside it in the same
run directory.

The same-process preflight and postflight evidence is frozen under:

`artifacts/phase-5c-database-compatibility-recovery/run-29472314013/live-same-process/`

The postflight files are:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `CampaignCompletion.log` | 12,552,438 | `219ac1c2de45f630c50106ffbe63a68778d941e5a1ec24b1a6952941d1e35235` |
| `completed_maps.json` | 1,260 | `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| `completed_maps.json.bak` | 951 | `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

## Completion boundary

The startup compatibility, zero-write recovery, one-record native victory
commit, atomic backup, same-process marker publication, scroll replay, and
normal shutdown gates are all satisfied. Phase 5C is complete and GO. Any
future completion is ordinary product behavior; further diagnostic deployment
or manual manipulation of the JSON files remains outside this approval.
