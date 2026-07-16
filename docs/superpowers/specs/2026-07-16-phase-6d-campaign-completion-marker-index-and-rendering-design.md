# Phase 6D campaign completion marker index and rendering design

Date: 2026-07-16 (Asia/Shanghai)

## Objective and present authorization

Phase 6D connects the Phase 6C.1 immutable campaign descriptor contract to the
existing completion database, marker index, PNG renderer, and public campaign
menu snapshots so a completed admitted campaign mission can display the same
check image at the end of its mission button.

The current approval covers this design only. It does not authorize
implementation, deployment, reading or writing the live completion database,
save access, campaign-progress access, process control, a live victory, or a
marker test. Those actions retain their separate boundaries.

Phase 6D must not add internal game calls, code patches, Hooks, synthetic input,
game-memory writes, Lua writes, campaign-progress writes, or a name/display
fallback. The installed Phase 6C.1 candidate remains unchanged until a later
audited deployment is explicitly approved.

## Inputs already accepted

- Phase 5B PNG loading, geometry, DirectDraw/GDI surface lifecycle, persistent
  redraw, first-entry rendering, and scroll replay are GO.
- Phase 5C database `0.5.0` compatibility, normal writable load without a
  startup write, atomic one-record commit, and same-process index publication
  are GO.
- Phase 6C.1 exact admitted descriptor and click/session/relative association
  is GO.
- The only identity authority remains confirmed same-MapInit-session
  `identity.relative`.

The currently admitted descriptor set is intentionally incomplete: Add-on
Trojan 1, Add-on Mayan 1, page-16 Roman 1/2, Original Viking 2/3, and Dark
Tribe 1–12. Pages 17–19, New World, New World 2, and all other unproven
controls must remain unmarked. Phase 6D does not infer or manufacture missing
descriptor records.

## Campaign-session completion gate

The Phase 6C.1 Trojan acceptance exposed a necessary correction. Its exact
descriptor matched while the independent presentation origin was
`random-map/eligible`. Passing that origin directly into the older completion
pipeline would record the campaign mission as an ordinary single-player map.

A new bounded `CampaignSessionAdmission` therefore owns only:

- one nonzero active MapInit session ID;
- the exact matched immutable descriptor key;
- the descriptor's exact relative identifier;
- an admitted boolean.

It becomes admitted only after all of these are true in the same session:

1. an exact public page/control/rectangle armed an admitted descriptor lease;
2. the next fresh MapInit session bound that lease;
3. confirmed `identity.relative` arrived for that exact session;
4. `ValidateCampaignDescriptor` returned `Matched` for the same key and exact
   relative;
5. the independent origin was not explicitly online/excluded.

Only then may the completion pipeline receive
`{LaunchSource::Campaign, SessionEligibility::Eligible}` for that session. A
prefix such as `Map\Campaign\`, a page number, a translated label, a display or
save name, an `RD` string, or temporal proximity cannot admit campaign
completion. Session change, descriptor mismatch, empty relative, online
origin, malformed snapshot, expiry, second click, page abandonment, or
shutdown clears the gate.

Loaded campaigns without a fresh descriptor click retain the established
`LoadCampaign` origin path and may canonicalize to `Campaign` only through the
existing load-origin contract. Phase 6D does not use an uncorrelated menu
visit to classify a loaded save.

## Campaign completion index

Add a separate `CampaignCompletionMarkerIndex` rather than weakening the
fixed-map `CompletionMarkerIndex` list-kind rules. Publication consumes an
owned `CompletionDatabaseSnapshot` and the already admitted immutable
descriptor catalog.

A database record is admitted to the campaign index only when:

- `mapKind == Fixed`;
- `launchSource == Campaign`;
- strict UTF-8 decoding succeeds within existing bounds;
- `stableId` exactly equals `BuildStableMapId(decoded relative)`;
- the decoded relative resolves to exactly one currently admitted descriptor;
- the decoded relative is exactly equal, including case and separators, to the
  descriptor's frozen relative;
- no second valid record resolves to the same descriptor key.

The index publishes an immutable set of descriptor keys/relatives under one
mutex and answers bounded, allocation-free lookup queries from the render
callback. `displayName`, translated menu text, save name, record order, page
position, and filename stem are never queried. Invalid, duplicate, ambiguous,
online, random, wrong-source, unknown-descriptor, and evidence-gap records are
silently excluded with one bounded summary diagnostic outside the draw loop.

The fixed-map index and its existing RD behavior remain unchanged. A player
save/display value such as `RD_PlayerSave` cannot affect either index. A true
confirmed relative beginning `RD_` remains a random-map completion and cannot
enter the campaign index.

## Public campaign marker observer

Add a `CampaignMarkerObserver` that consumes only successful owned
`CampaignMenuSnapshot` values plus the immutable catalog and campaign index.
For every visible feature it requires:

- nonempty bounded public text, used only to reject navigation/decorative
  controls;
- exact snapshot page;
- exact control ID and rectangle;
- a uniquely admitted immutable descriptor record;
- a unique completed descriptor-key match in the campaign index;
- nonzero logical surface dimensions and checked geometry bounds.

The observer emits a draw command from the public feature's logical surface and
button rectangle. It never uses the text as an identity or database key. A
successful new snapshot atomically replaces the retained page frame; an empty,
invalid, ambiguous, over-capacity, page-changing, or non-campaign snapshot
clears it. Hover/effects-only updates retain the same identity and geometry.

The observer supports at least 12 commands so the admitted Dark Tribe page
cannot be truncated. The shared marker frame type should use a neutral maximum
such as 16 commands; `FixedMapRowObserver` continues to emit at most six.

## Render scheduling and geometry

Reuse the accepted embedded check PNG, `DirectDrawMarkerSurface`, and checked
`BuildMarkerCheckGeometry`. The marker remains anchored at the mission button's
right end, with pillarbox adjustment applied exactly once and clipping to the
button/destination surface.

On each public UI-frame callback:

1. compute the deterministic active campaign owner already used by public
   capture;
2. finalize any changed public snapshot;
3. publish it to both launch association and campaign marker observer;
4. render campaign commands only when the callback page equals that exact
   owner;
5. otherwise clear/skip the campaign frame;
6. preserve the existing fixed-map page-25 render path unchanged.

There is one shared drawing surface and one renderer call per eligible frame.
The first stable page epoch must display without mouse hover. Re-entry,
effects-only repaint, and any campaign menu scroll that produces a fresh public
snapshot must replace and redraw commands immediately. Rendering failure uses
the existing three-failure disable policy and never affects navigation,
identity, completion admission, or persistence.

## Runtime composition

The proposed `0.10.0` candidate restores only previously accepted production
components plus the new campaign-specific gate/index/observer:

- compatible `CompletionStore` load and owned snapshot publication;
- existing bounded worker and atomic commit path;
- existing native terminal event `609` admission;
- fixed-map marker index/observer/renderer behavior;
- Phase 6C.1 descriptor catalog and association;
- new campaign-session completion gate;
- new campaign marker index and public observer.

Startup must publish the loaded database snapshot without writing it. A
compatible unchanged database produces no normalization or commit. Any
read-only/failure store mode may publish its valid read-only snapshot for
markers but cannot accept completion writes. Campaign completion is written
only after a later, explicitly authorized live victory reaches the exact gate
above.

Shutdown ordering is:

1. stop accepting completion candidates;
2. disable campaign/fixed observers and both indexes;
3. disable completion, descriptor, identity, and native-event admission;
4. drain the worker within the existing bounded timeout;
5. close the callback gate and remove public listeners in reverse order;
6. release the renderer surface and store-owned objects.

No callback may retain a pointer to a destroyed catalog, snapshot, index, or
drawing surface.

## RED/GREEN test contract

Implementation approval should begin with local RED tests and keep intermediate
RED commits local. Tests must prove at least:

- a valid `Campaign` record with self-consistent stable ID and exact admitted
  relative publishes one descriptor key;
- wrong source, random kind, online source, malformed UTF-8, wrong stable ID,
  `mcd2_roman1`, case/prefix-only guesses, evidence-gap relatives, and duplicate
  descriptor records do not publish;
- `RD_PlayerSave` in display/name fields cannot affect a correct or incorrect
  result;
- exact descriptor match promotes only that same session to Campaign for
  completion; the Trojan `random-map` presentation origin regression becomes
  Campaign only after the exact descriptor match;
- explicit online origin, session mismatch, wrong relative, and descriptor
  rejection never promote or write;
- a successful Trojan snapshot emits one command at
  `320,200,175,30` without hover;
- page-16 Roman and Original controls emit only for completed admitted
  descriptors;
- a 12-control Dark Tribe page is not truncated;
- invalid snapshot, page change, re-entry, effects-only repaint, and changed
  visible controls replace or clear retained commands correctly;
- pillarbox, scaling, clipping, overflow, PNG lifecycle, rendering failure,
  fixed-map markers, same-process index publication, and shutdown order retain
  their existing GREEN tests;
- source policy rejects internal calls, process-memory writes, Hooks, patches,
  synthetic input, Lua writes, campaign-progress access, display-name identity,
  and database manipulation outside the established store transaction.

Complete MSVC Win32 CI, mutation fixtures, full tests, PE32/export verification,
package audit, and candidate hashes remain mandatory before any deployment
request.

## Future live authorization and acceptance

Implementation and artifact audit alone perform no live database access. A
future live acceptance requires two new explicit approvals:

1. exact guarded deployment approval while both protected applications are
   normally closed;
2. after deployment, explicit approval to read the existing completion
   database and allow one campaign victory to add at most one record through
   the established atomic store transaction.

The efficient one-process acceptance is:

1. preflight exact archive/ASI/INI and database main/backup bytes, timestamps,
   hashes, record count, and absence of temporary siblings;
2. start at the main menu and require compatible store load with no startup
   write;
3. enter Add-on Trojan 1, require exact descriptor/session match, and allow its
   automatic victory to commit exactly one `Campaign` record;
4. return to the Trojan page without hover and require the PNG marker to appear
   at control `91` immediately and remain stable across repaint/re-entry;
5. require no second commit, no rejected descriptor promoted to completion,
   and no marker on an uncompleted/evidence-gap control;
6. close normally and verify the main database gained exactly one record, the
   backup equals the exact pre-commit main, all earlier records are preserved,
   installed files remain exact, and no temporary sibling remains.

If Trojan 1 is already present by exact stable ID at acceptance time, the
transaction must be `Duplicate` with no write; choose another admitted mission
only through a new explicit live-write approval. No manual JSON edit, restore,
save manipulation, or game-progress modification is implied.

## Completion boundary

Phase 6D GO would cover only the currently admitted descriptor subset and the
exact campaign completion/marker contract. It would not make all campaign
missions complete. Expanding markers to pages 17–19, New World, New World 2,
or any other missing control requires closing each descriptor's own static and
public-geometry evidence chain in a later separately approved phase. Missing
missions stay unmarked rather than inferred.
