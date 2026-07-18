# Phase 6A.1 public sparse campaign association design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Close only the public click-to-session association gap demonstrated by the
Phase 6A live run. Phase 6A.1 does not claim a complete no-hover campaign-menu
catalog and does not authorize campaign marker rendering.

## Frozen live contract

The page-21 GUI callback behaves as a sparse state-change stream:

- `Mission 3` appeared as control `2083`, rectangle `500,168,175,30`;
- hover changed only `effects` between `0` and `2`;
- every successful one-feature callback interval was followed by an empty
  interval about 30 ms later;
- click-time volatile redraws caused a conflicting per-frame snapshot;
- MapInit occurred after page 21 transitioned through briefing page 37;
- SU confirmed session 1 as `Map\Campaign\dark03.map`.

Those adjacent observations are not an approved mapping because the Phase 6A
association state machine never armed.

## Public sparse cache

`CampaignMenuCapture` becomes a bounded page-residency cache keyed by exact
public control ID and rectangle. Empty callback intervals publish nothing and
do not erase the cache. An exact duplicate is ignored. A callback whose only
change is `effects` updates the cached volatile state. Any other disagreement
for one key, malformed text, capacity overflow, ambiguous page admission, or
shutdown still invalidates and clears the catalog.

Leaving page 21 clears the catalog. No feature survives into a later visit.
The cache owns all copied values and retains no game pointer.

## Launch lease

Only a unique exact control match with non-empty bounded label text may arm a
mission launch candidate. Text presence is a structural eligibility check;
its content never identifies or classifies the map.

Once armed, the candidate survives page 21 leaving and later invalid redraws
for at most 30 seconds so the observed page-37 briefing transition can reach
MapInit. A non-campaign origin, wrong session, timeout, another page-21 click,
return to page 21 without MapInit, empty SU relative identity, or shutdown
clears it. Only the exact same-session confirmed `identity.relative` may be
emitted.

The player-chosen name remains irrelevant. A name such as `RD_PlayerSave`
cannot create random-map classification; only the confirmed session-bound
relative identity is authoritative.

## Enforced boundary

Phase 6A.1 remains public-S4ModApi-only and structurally read-only. Runtime
startup constructs no completion store/worker/admission, native victory
subscriber, internal menu adapter, fixed-map or campaign marker renderer, game
hook, process patch, synthetic input, Lua write, or game-data write.

The main and backup JSON must remain byte-identical through any live run.
Development, CI artifact audit, and deployment remain separate gates. A fresh
explicit approval with both protected applications closed is required before
replacing the Settlers United archive.

## Acceptance boundary

Phase 6A.1 is GO only for the association subproblem when the diagnostic logs,
in order, a unique armed public control, eligible campaign MapInit session, and
one `campaign-menu-association` containing the same session's exact SU
`identity.relative`, with zero database changes.

Even that GO leaves the full no-hover menu catalog as an evidence gap. Any
private campaign-menu reader requires a separate static-evidence design and
explicit approval.
