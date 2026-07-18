# Phase 6D gap-only all-campaign geometry report

Date: 2026-07-18 (Asia/Shanghai)

## Decision

The single-process gap-only public catalog batch is **GO**. It closes public
geometry for all 107 statically proven campaign mission descriptors in the
accepted executable.

This is geometry/catalog GO only. It does not authorize completion-database or
save access, campaign completion classification, mission launch, victory
handling, marker indexing/rendering, or any internal menu adapter.

## Frozen live input

- Candidate version: `0.10.0`.
- Runtime mode: `phase-6d-gap-only-campaign-catalog`.
- Loaded candidate ASI SHA-256:
  `de36bfca5cfd3f0cf36a063e7717fe1a20c20754bff2f1bcc6d39158dc113a6c`.
- Exact executable SHA-256:
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Log byte range: `[19621126,19679027)`.
- Slice size: `57901` bytes.
- Slice SHA-256:
  `971f0bc7f7a4797334f6e76ef84e025e5c279683712ef0efbb4f68e6ab85cb82`.
- Session process ID: `27160`.

The user performed one uninterrupted traversal from the main menu through
every accessible campaign subpage and scroll state, launched no mission, and
returned to the main menu. The frozen slice contains no MapInit, Lua identity,
campaign association, session binding, warning, error, empty snapshot, or
invalid snapshot.

## Coverage

The source ID `L6D-G` in the descriptor matrix means the exact public
page/control/rectangle observed in this frozen Phase 6D gap-only slice.

| Public owner page | Content | Exact controls | Unique controls | Snapshots | Result |
| ---: | --- | --- | ---: | ---: | --- |
| 4 | Mission CD Bonus | `1919` | 1/1 | 13 | CLOSED |
| 6 | New World + New World 2 composite | `1825–1844`, `2939–2954` | 36/36 | 6 | CLOSED |
| 11 | Add-on Trojan | `91–102` | 12/12 | 3 | CLOSED |
| 12 | Add-on Roman | `47–50` | 4/4 | 1 | CLOSED |
| 13 | Add-on Mayan | `34–37` | 4/4 | 2 | CLOSED |
| 14 | Add-on Viking | `112–115` | 4/4 | 2 | CLOSED |
| 15 | Add-on Settle | `77–80` | 4/4 | 1 | CLOSED |
| 16 | Mission CD Roman | `1903–1907` | 5/5 | 2 | CLOSED |
| 17 | Mission CD Viking | `1950–1954` | 5/5 | 1 | CLOSED |
| 18 | Mission CD Mayan | `1889–1893` | 5/5 | 1 | CLOSED |
| 19 | Mission CD Settle + Conflict | `1941–1946` | 6/6 | 5 | CLOSED |
| 20 | Original Viking/Mayan/Roman | `2038–2046` | 9/9 | 4 | CLOSED |
| 21 | Dark Tribe | `2081–2092` | 12/12 | 5 | CLOSED |
| **Total** |  |  | **107/107** | **46** | **CLOSED** |

Every snapshot status was `success`. No snapshot contained an unexpected
mission control. Repeated observations of the same page/control pair produced
one and only one logical rectangle, including after scroll and re-entry.

The page-4 direct bonus exception is now closed at public rectangle
`427,370,277,127`. The two composite New World groups are both closed under
deterministic owner page 6; they are not assigned to page 5 from presentation
state.

## Matrix audit

The merged row-level artifact is
[`research/evidence/phase-6d-all-campaign-descriptor-matrix.tsv`](../../research/evidence/phase-6d-all-campaign-descriptor-matrix.tsv).

- Matrix SHA-256:
  `35c83e1dc89542d88e30d41209a187ac2aff0ba185dec1eb9e5029194ec6d864`.
- Rows: `107`.
- Unique descriptor keys: `107`.
- Unique page/control keys: `107`.
- `CLOSED`: `107`.
- `EVIDENCE GAP`: `0`.
- All rectangles are nonempty and within the logical `800x600` surface.

The merge used exact page/control keys only. It did not infer a rectangle from
labels, translated text, neighboring controls, apparent ordering, display
names, save names, or database content.

## Identity and RD boundary

No runtime identity was produced or required during this menu-only batch.
Static relatives remain descriptor constraints, not observed completion
identities.

Any future classification remains valid only when a fresh exact descriptor
lease is followed by the same MapInit session's confirmed
`identity.relative`. `identity.name`, display/save names, database fields, and
presentation text beginning with `RD` remain non-authoritative. Only the exact
confirmed session-bound relative may establish random-map status.

## Next boundary

The unified descriptor and public geometry matrix is ready for design review
of the common all-campaign completion index, bounded completion source,
snapshot observer, and PNG renderer. That implementation requires a separate
explicit approval.

## Normal-shutdown postflight

After the user normally closed the game and Settlers United:

- protected-process count: `0`;
- final session log range: `[19621126,19694279)`;
- final session log size: `73153` bytes;
- final session log SHA-256:
  `839ff8933df1a6a6ec227c3edee405914471be06b2d7b387793060676ae7bb70`;
- final full log size: `19694279` bytes;
- final full log SHA-256:
  `f81d3bdd3368453bb24d1225e2fee3424cdc3444117c1f5a2054a2d6fce8c66c`.

The installed archive remained `1400259` bytes with SHA-256
`ea15116f8b84996c4c72e037d6b41782c4921f91224eec24c6acdc1d40e9ac67`.
Its unique embedded project ASI retained SHA-256
`de36bfca5cfd3f0cf36a063e7717fe1a20c20754bff2f1bcc6d39158dc113a6c`,
and the live INI retained SHA-256
`4a9835f5ea2228aea2780e2d393f001d87e09c35dd87f6525969cc69a0b01e38`.

Database main and backup retained their exact preflight sizes, timestamps, and
hashes. Archive, INI, and database temporary siblings were all absent.

Phase 6D public catalog geometry is therefore **GO**. This GO does not
authorize database/save access, completion classification, marker
implementation/rendering, or another deployment.
