# Phase 5C database compatibility startup-recovery report

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The Phase 5C code-only database compatibility correction is **GO for startup
recovery**. The audited candidate loaded the untouched three-record primary
database in writable mode, published the historical TheCross completion to the
fixed-map list, and exited without changing the primary database, backup, or a
database temporary file.

This is not full Phase 5C completion. The separately authorized same-process
victory commit and immediate-marker-refresh acceptance remains outstanding.
This report authorizes no live database write, restoration, manual database
replacement, or further deployment.

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

## Frozen live evidence

The complete project log after normal exit is copied beneath the ignored
artifact tree at:

`artifacts/phase-5c-database-compatibility-recovery/run-29472314013/live-startup-recovery/CampaignCompletion.log`

It is 12,467,773 bytes with SHA-256
`0f5a1aa324a608838209ed896e32abe5e7d8dd6f23cd483719e08c1946f69855`.
The candidate audit and guarded deployment result remain beside it in the same
run directory.

## Remaining Phase 5C gate

Startup recovery is now closed as GO. Full Phase 5C still requires a fresh,
explicitly approved live-write test in a new process: one eligible previously
uncompleted fixed map must receive one real local-player victory event, produce
exactly one fourth primary record, refresh its marker without restart, and
leave the backup equal to the exact three-record pre-commit primary. No part of
this startup-recovery approval implies permission for that database write.
