# Phase 2.2 Fixed-Map Tab Calibration

## Scope

This calibration attributes only the three tabs shown while the exact public
UI page snapshot is `4,22,23,25`. It does not infer a selected map or read game
memory. The capture came from the user-driven 2026-07-13 session using
`CampaignCompletionDebug.asi` SHA-256
`867efcf827836e9f5d25338536eb58f278ecbd38b6a94c9469ff38c3a1c49075`.

## Accepted observations

| Row | User action | Local timestamp | Element ID | Active pages | Decision |
| --- | --- | --- | ---: | --- | --- |
| C1 | Click Single Player Maps while it was already the default tab | `2026-07-13T15:13:56.201+LOCAL` | `2449` | `4,22,23,25` | Accept as `single` |
| C2 | Click Multiplayer Maps and allow the list to change | `2026-07-13T15:14:57.146+LOCAL` | `2450` | `4,22,23,25` | Accept as `multiplayer` |
| C3 | Click Custom Maps and allow the list to change | `2026-07-13T15:15:22.840+LOCAL` | `2451` | `4,22,23,25` | Accept as `custom` |

The approved numeric initializer in `single`, `multiplayer`, `custom` order is
therefore `{2449, 2450, 2451}`.

## Negative controls

| Row | User action | Local timestamp | Element ID | Active pages after action | Result |
| --- | --- | --- | ---: | --- | --- |
| N1 | Single-click one Custom Maps map row | `2026-07-13T15:16:19.471+LOCAL` | `2419` | `4,22,23,25` | Distinct from all accepted tab IDs |
| N2 | Click Back | `2026-07-13T15:16:48.881+LOCAL` | `2415` | Transitions through `4,7,22,23,25` to `7` | Distinct from all accepted tab IDs and leaves the fixed-map page set |

No accepted ID fired for a different tab, the tested map row, or Back. Runtime
attribution must still require the exact page snapshot and must fail closed to
`Unknown` when a mapping is ambiguous or the page snapshot changes.
