# Phase 6C immutable campaign descriptor and anchor design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Convert the Phase 6B public page catalogs and frozen executable evidence into
independently admitted immutable campaign descriptors, while minimizing live
mission launches. Prove the complete chain from a public mission control to an
exact relative identifier before any campaign marker is considered.

This design and its offline evidence work do not authorize runtime
implementation, deployment, marker rendering, database writes or restoration,
save access, campaign-progress edits, synthetic input, or game launch.

## Why Phase 6B is not yet a marker contract

Phase 6B proved bounded public sparse capture and deterministic page isolation
for pages 3, 4, 6, and 11 through 21. A sparse public feature is still only a
draw observation. Its label, texture, ordinal on screen, or neighboring page
does not prove which map it launches.

The static path-format table likewise constrains possible identifiers but does
not connect one public `valueLink` to one formatter and ordinal. Marker code
must not join those two partial facts by position or translated text.

## Descriptor unit and evidence chain

The smallest independently gated descriptor is one mission-list implementation
with one exact formatter selection rule. Its immutable records contain:

```text
descriptor key
public page ownership
public valueLink
logical 800x600 rectangle
zero-based mission ordinal
exact formatter slot and format literal
exact relative identifier
```

Admission requires one continuous static chain in the exact approved
`S4_Main.exe`:

1. the state constructor admits a bounded public control range;
2. each mission control selects one exact internal action and ordinal;
3. the click dispatcher accepts that action and preserves the ordinal;
4. the transition sink selects one exact campaign formatter slot;
5. the frozen formatter literal and numbering rule produce one relative path;
6. public page and geometry observations agree with the immutable record;
7. one representative launch for each distinct proven transition
   implementation agrees with the same MapInit session's confirmed
   `identity.relative`.

Every selected code and literal window has a frozen RVA, length, and SHA-256.
An unreadable or mismatching window disables only the affected descriptor.
Missing geometry or an ambiguous control also leaves that descriptor disabled.

## Shared transition proof and anchor reduction

Code reuse may reduce dynamic anchors only when the exact executable proves the
same transition sink, ordinal encoding, and formatter-slot selection. Merely
having similar menu layouts or path prefixes is insufficient.

Preliminary Add-on analysis illustrates the rule. Its Roman, Mayan, Viking,
Settle, Trojan, and Bonus state handlers use separate bounded public control
ranges and distinct formatter-slot constants, but converge on a common mission
transition entry. Once every construction/dispatch window and rectangle is
frozen, one selectable Add-on representative may anchor that shared transition
implementation. It does not turn an incomplete sibling descriptor into GO.

The same sharing audit is performed independently for Mission CD, New World,
New World 2, and the original campaigns. The final manifest contains one launch
per distinct proven transition implementation, not one launch per mission or
one launch per translated campaign label. If sharing cannot be proven, the
implementations remain separate or disabled.

Dark Tribe's accepted same-session anchor
`2083 -> Map\Campaign\dark03.map` is reused unless shared association or
transition code changes. It is never repeated merely to increase sample count.

## Offline analysis deliverable

Before runtime development, produce a frozen evidence report with:

- exact EXE/PDB and prior live-slice identities;
- construction, dispatch, transition, formatter, and literal windows;
- per-descriptor control ranges, ordinals, rectangles, and relative paths;
- the exact proof for every claimed shared transition implementation;
- a per-descriptor decision of `STATIC READY`, `EVIDENCE GAP`, or
  `INCOMPATIBLE`;
- a minimal dynamic anchor manifest containing only currently selectable
  representatives.

Candidate mappings discovered during analysis remain non-admitted until the
entire chain is frozen. The report must distinguish a candidate control range
from an authorized descriptor.

## Read-only diagnostic candidate

After separate development approval, the Phase 6C candidate may add only:

- exact-version and immutable-window admission;
- compile-time descriptor tables for `STATIC READY` entries;
- public sparse ID/rectangle cross-checks;
- bounded click-to-session leases through the existing campaign association;
- comparison against the confirmed same-session `identity.relative`;
- deterministic diagnostic records in the existing project-owned log.

It must keep the completion store, worker, native victory path, marker index,
and renderer disabled. It must not call reviewed internal functions, read
private mutable campaign state, patch code, inject input, read or change
campaign progress, inspect saves, or modify completion JSON.

## Efficient live acceptance

Use one audited deployment and one game process. Never run campaign tests in
parallel because the installed archive, log, database, and session stream are
shared.

1. Freeze package, INI, database main/backup bytes and timestamps, log offset,
   process count, and temporary siblings once.
2. Visit only pages named by the generated manifest; navigation may be one
   uninterrupted batch with no per-page acknowledgment.
3. Launch one currently selectable representative for each distinct proven
   transition implementation, returning normally between launches.
4. Require fresh monotonically increasing MapInit sessions and exactly one
   association per launch.
5. Return to the main menu, close normally, and perform one postflight.

Automatic AI Resign is accepted as existing behavior. Locked content is logged
as unavailable and is never unlocked, edited, or replaced with a save. If a
representative is unavailable, that dynamic anchor remains an evidence gap.

## Identity and RD boundary

The only dynamic classification authority is the confirmed same-session
`identity.relative`. `identity.name`, menu labels, database display fields,
player save names, save filenames, and strings containing `RD` are never inputs
to descriptor selection or random-map classification.

The static descriptor predicts an exact relative path. The live anchor may
only confirm or reject that prediction. It may not rewrite the prediction or
fall back to presentation data.

## Acceptance and next phase

A descriptor becomes `GO` only when its complete static chain, public
cross-check, and required shared-transition anchor all agree. Failure is local
and fail-closed. Database main and backup files and their timestamps must remain
byte-identical, with zero protected processes and no temporary siblings after
shutdown.

Phase 6C `GO` authorizes only the immutable descriptor contract. Campaign
marker indexing, immediate rendering, redraw persistence, and any storage or
victory integration remain a separate Phase 6D design and explicit approval.
