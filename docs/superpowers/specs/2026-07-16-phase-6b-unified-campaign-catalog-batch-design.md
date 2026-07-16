# Phase 6B unified campaign catalog and batch acceptance design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Replace campaign-by-campaign diagnostic repetition with one exact-version-
gated catalog framework and one sequential, single-process acceptance batch.
Cover original, Add-on, Mission CD, New World, and New World 2 content without
weakening identity, deployment, database, save, or process boundaries.

This design does not authorize runtime implementation, deployment, marker
rendering, database writes or restoration, save access, campaign-progress
changes, synthetic input, or game launch.

## Common descriptor

Each independently admitted descriptor contains only immutable records:

```text
descriptor key
public page set
exact executable window fingerprints
entry ordinal
public control ID
logical 800x600 rectangle
exact relative identifier
```

The framework owns compatibility checking, public sparse-cache cross-checking,
bounded click leases, and same-session association. Descriptors own evidence.
A shared C++ code path must not make an unproven page inherit another page's
control IDs, geometry, count, or path numbering.

Dark Tribe enters the framework as the already proven baseline descriptor.
All other descriptors start disabled. There is no partially populated table and
no best-effort label parser.

## Descriptor admission

A descriptor becomes diagnostic-ready only when all of these are frozen:

1. exact approved EXE identity;
2. exact immutable control-construction and click-dispatch windows;
3. bounded ordinal and formatter selection for every admitted control;
4. exact path-format initializer window;
5. one complete public geometry snapshot or an exact static geometry proof;
6. one selectable entry dynamically associated to the same MapInit session's
   confirmed `identity.relative`.

An EXE/window mismatch, duplicate ID, overlapping ordinal, page disagreement,
unexpected public rectangle, missing session identity, or ambiguous click
disables the affected descriptor. It cannot borrow an adjacent page, state,
session, path prefix, or translated label.

## Read-only calibration candidate

The first implementation candidate after separate approval should add only
bounded diagnostics for all campaign public pages:

- retain sparse public features only while their exact page residency is live;
- emit one deduplicated page catalog summary after entry, scroll, and return;
- arm a lease only from one unique exact public campaign control;
- associate it only to the next qualifying same-session MapInit identity;
- emit descriptor-key, page, control, rectangle, session, and confirmed
  `identity.relative` for offline comparison;
- write only the existing project-owned diagnostic log.

It must not render a marker, subscribe a new victory path, read a private
mutable menu collection, call a reviewed internal game function, patch code,
inject input, inspect campaign progress, or touch the completion database.

## Single-launch catalog pass

Do not run game processes concurrently. The installed SU archive, plugin log,
completion database, and session stream are shared, so parallel processes make
attribution unsafe.

Use one audited candidate, one launch, and one continuous log. The user may
navigate all accessible campaign families consecutively without sending a
message after each page:

1. visit Add-on selector page 3 and accessible child pages 11 through 15;
2. visit Mission CD selector page 4 and accessible child pages 16 through 19;
3. visit New World page 5;
4. visit New World 2 page 6;
5. visit original campaign page 20 and Dark Tribe page 21;
6. on each scrollable page, enter once, scroll enough to force a redraw, return
   to its top, then leave normally;
7. return to the main menu and report the catalog pass complete once.

Locked or unavailable pages are logged as unavailable. They are not unlocked
and do not fail the batch. No hover gesture is required for acceptance.

## Minimal launch-anchor pass

After offline analysis groups pages by exact construction/dispatch
implementation, launch only one currently selectable representative for each
still-unanchored descriptor implementation. Do not launch every mission.

The batch manifest is generated before deployment and lists exact page,
control, and expected path family. The user performs all listed launches in the
same game process, returning normally between them, and reports only once at
the end. Each launch must produce a fresh monotonically increasing session and
exactly one association. Automatic AI Resign is accepted as existing behavior.

Dark Tribe's accepted `2083 -> Map\Campaign\dark03.map` anchor is reused. It is
included only as one final regression control if shared association code
changes. A formatter variant such as Add-on bonus is not assumed equivalent to
a numbered formatter; offline dispatch evidence must either prove its mapping
or leave it disabled.

This turns the live burden from “every campaign mission” into “one catalog
navigation pass plus at most one launch per distinct proven menu
implementation.” Exact static sharing may reduce the launch count further.

## Preflight and postflight

Preflight is performed once for the batch:

- require both protected processes closed;
- record the audited artifact, installed archive and INI identities;
- record completion-database main/backup bytes, sizes, hashes, and timestamps;
- record the diagnostic log start offset and reject stale temp siblings.

Deployment still requires a fresh explicit user approval naming the exact
Phase 6B candidate. Codex never terminates the game or Settlers United.

Postflight is also performed once, after the user closes normally:

- freeze the log and verify catalog/association invariants;
- require zero protected processes;
- require byte-identical database main and backup, unchanged timestamps, and
  no temporary siblings;
- verify the installed package still embeds the exact audited candidate.

No database restore is implied by a mismatch. Any restore, database edit, or
save access requires its own exact approval.

## Acceptance matrix

| Gate | Requirement |
| --- | --- |
| Compatibility | Exact EXE and every descriptor window match |
| Catalog | Complete bounded records for each descriptor claimed GO |
| Redraw | Entry, scroll, and return do not require hover to republish |
| Association | One exact control to one fresh session-bound relative identity |
| Isolation | No cross-page, cross-descriptor, or cross-session promotion |
| RD rule | Classification uses only confirmed session-bound `identity.relative` |
| Zero write | Database main/backup and timestamps remain identical |
| Process | One game process only; normal user-controlled close |

Results are per descriptor: GO, EVIDENCE GAP, or incompatible. One missing
family does not authorize guessing and does not invalidate already proven
Dark Tribe evidence. A diagnostic GO still does not authorize campaign marker
rendering; that remains a separate reviewed implementation and deployment
decision.
