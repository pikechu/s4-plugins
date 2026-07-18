# Phase 6A Dark Tribe campaign-menu read-only forensics design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Establish a language-independent, session-confirmed mapping between visible
Dark Tribe campaign mission controls and the exact map-relative identities
returned after launch. This is the first evidence gate for campaign-page
completion markers.

Phase 6A is diagnostic-only. It does not render a campaign marker, create or
change a completion record, edit campaign progress, unlock a mission, synthesize
input, or infer map identity from localized text.

## Why Dark Tribe is first

The approved project design orders campaign work with Dark Tribe first. Public
S4ModApi declares the dedicated page
`S4_SCREEN_SINGLEPLAYER_DARKTRIBE == 21`, and the frozen historical log already
contains user-driven page-21 GUI and mouse observations. The approved PDB also
contains the `CStateCampaignDark` state and its callback functions.

That evidence proves the page exists and is observable, but it does not prove
which mission control launches which `identity.relative`. Existing rate-limited
GUI logs captured only isolated controls and cannot support rendering.

## Current implementation boundary

The production marker path is deliberately fixed-map-specific:

- it admits only the exact page set `{4,22,23,25}`;
- it reads only the fixed-map menu model;
- `CompletionMarkerIndex` publishes only validated `Map\Singleplayer`,
  `Map\Multiplayer`, and `Map\User` records whose source agrees with the list;
- campaign completion records can already be persisted, but are intentionally
  absent from the marker index.

Phase 6A must not weaken any of those checks or reuse a fixed-map row signature
for a campaign page.

## Public-only diagnostic capture

The first candidate uses only existing public S4ModApi page, GUI-element,
mouse, MapInit, tick, and Lua-open observations. It adds no internal memory
reader, function hook, code patch, process-memory API, DirectDraw hook, cursor
movement, or synthetic input.

A bounded `CampaignMenuCapture` owns all diagnostic state. It opens a Dark
Tribe observation epoch only while public page observations admit page 21. It
copies GUI callback values into project-owned storage and immediately discards
all game-owned pointers.

Each copied feature contains only:

- logical surface dimensions;
- container type and rectangle;
- `valueLink`, main/pressed/back/show textures and tooltip links;
- image style, effects, and text style;
- bounded, validated display text when public callbacks supply it.

The collector holds at most 128 distinct features and at most 256 bytes of text
per feature. A null pointer, malformed text, conflicting duplicate, capacity
overflow, ambiguous page epoch, or callback-order conflict rejects the whole
snapshot. It never truncates a snapshot and calls it valid.

The GUI callback has no page argument. Therefore page ownership must not be
derived from the existing one-second diagnostic `currentPage_` value. The
collector uses page-specific frame callbacks plus public
`IsCurrentlyOnScreen(21)` admission and records callback ordering. If live
evidence cannot establish an unambiguous frame boundary, Phase 6A remains an
evidence gap; it does not add a private reader merely to force a result.

## Launch association

A `CampaignLaunchAssociation` may retain one bounded click candidate only when:

1. the user releases the left button on page 21;
2. the mouse control ID and rectangle uniquely match one feature in the latest
   admitted snapshot;
3. the next launch transition remains within the campaign-origin state machine;
4. MapInit begins a new session;
5. Settlers United returns a validated, session-bound relative identity for
   that same session.

Only after all five conditions agree may the diagnostic emit:

```text
campaign-menu-association page=21 control=<id> rect=<x,y,w,h> relative=<validated path>
```

The control ID, row position, display text, save name, campaign title, or map
filename stem never creates identity. The emitted relative path is evidence for
a future reviewed mapping catalog; it is not promoted into completion state by
this candidate.

## RD archive-name rule

Random-map classification continues to use only the confirmed session-bound
`identity.relative`. A save or display name beginning with `RD` or `RD_` has no
effect. Phase 6A does not classify from campaign labels and does not broaden
the accepted fixed/campaign identity policy.

## Enforced read-only runtime mode

The diagnostic candidate must enforce read-only behavior in production code,
not merely advertise it in INI text:

- completion storage worker and admission are not started;
- native terminal-event subscription is not started;
- fixed-map and campaign marker renderers are not started;
- no completion JSON transaction method is reachable;
- the main and backup JSON may be read only to freeze/verify their hashes, or
  may be left unopened if the diagnostic does not need them;
- no database temporary sibling is created.

This is required because the local AI Resign test flow can trigger victory
automatically after a mission launch. A diagnostic mission launch must still
leave both JSON files byte-identical.

The packaged policy must state `CompletionDetection=0`, `CompletionStorage=0`,
`CompletionMarkers=0`, `NativeEventSubscription=0`, and
`InternalMenuRendering=0`, with source-level policy tests proving those values
match runtime construction. The persistence writer version remains `0.6.0`;
historical record versions must not be rewritten.

## Required automated tests

Test-first development must prove:

1. only an admitted page-21 epoch can publish a Dark Tribe snapshot;
2. feature copying is bounded, lossless, deterministic, and pointer-free;
3. duplicate callbacks deduplicate only when every copied field agrees;
4. rectangle/control collisions, invalid text, overflow, and ambiguous callback
   ordering reject the snapshot;
5. leaving page 21 clears pending features and click candidates;
6. only a unique left-button release can arm launch association;
7. session mismatch, missing MapInit, non-campaign origin, identity timeout, and
   conflicting identities fail closed;
8. display/save values such as `RD_PlayerSave` cannot replace or classify the
   confirmed relative identity;
9. no association creates a completion candidate or marker command;
10. read-only diagnostic startup cannot construct a completion writer, native
    victory subscriber, or marker renderer;
11. all existing victory, persistence, fixed-map marker, database compatibility,
    random-map, online exclusion, and shutdown tests remain GREEN;
12. mutation fixtures continue to reject process writes, code patches, Hook
    frameworks, Lua writes, game-data writes, and victory-condition calls.

Intermediate RED commits remain local. Only a complete GREEN checkpoint is
pushed, and authoritative Windows Release CI is required before any deployment
request.

## Static evidence checkpoint

Before implementation is considered deployment-ready, freeze:

- approved EXE and PDB hashes;
- exact S4ModApi page enum values;
- relevant `CStateCampaignDark` PDB records and reviewed disassembly windows;
- the historical page-21 GUI/mouse excerpt;
- source proof that the public GUI callback does not supply a page number;
- source proof that the diagnostic runtime disables every persistence and
  rendering construction path.

PDB names and disassembly are navigation evidence only. They do not authorize
calling an internal campaign function or reading an unreviewed private layout.

## Live acceptance

Deployment requires a separately audited artifact, closed game/SU processes,
and fresh explicit user approval. The existing guarded archive procedure may
replace only the project ASI and INI.

The user performs all launch, navigation, mission selection, return, and exit
actions. The assistant never terminates either process.

The first live sequence is:

1. freeze the four-record main JSON and three-record backup hashes;
2. start the read-only candidate and require its exact ASI plus compatible EXE;
3. enter the Dark Tribe page without hovering a mission and capture a stable
   bounded snapshot;
4. exercise hover/selection/back behavior and require unchanged input;
5. launch one available mission through a uniquely captured control;
6. require MapInit and Settlers United to confirm one exact relative identity;
7. return normally, revisit page 21, and require the snapshot to reproduce;
8. exit normally and prove both JSON files and their timestamps are unchanged,
   no database temporary file exists, and no completion commit occurred.

Automatic AI Resign may occur after mission entry; it must have no persistence
effect because the candidate is structurally read-only.

## GO / evidence-gap boundary

Phase 6A is GO only for campaign-menu forensics when the page-21 snapshot is
stable without hover, one user-selected control uniquely associates with one
confirmed `identity.relative`, navigation remains responsive, and the database
is byte-identical after exit.

GO does not authorize campaign marker rendering. A separate Phase 6B design
must define the reviewed Dark Tribe mapping catalog, row geometry, completion
index rules, locked/unavailable mission behavior, drawing integration, and
same-process refresh acceptance. If any association is ambiguous or the public
callback ordering is insufficient, record an evidence gap and design the
smallest additional read-only probe; do not guess or broaden identity rules.
