# Phase 6D unified all-campaign evidence, completion index, and batch rendering design

Date: 2026-07-16 (Asia/Shanghai)

## Objective and present authorization

Phase 6D closes the remaining campaign descriptors as one evidence set, then
connects that set to the existing completion database, PNG marker renderer, and
public campaign snapshots in one implementation candidate. The user should not
have to validate one campaign family at a time or repeat the same menu test for
each mission.

The current approval covers this revised design only. It does not authorize
implementation, deployment, reading or writing the live completion database,
save access, campaign-progress access, process control, a live victory, or a
marker test. Those actions retain their separate boundaries.

Phase 6D must not add internal game calls, code patches, Hooks, synthetic input,
game-memory writes, Lua writes, campaign-progress writes, or a name/display
fallback. The installed Phase 6C.1 candidate remains unchanged until a later
audited deployment is explicitly approved.

## Unified scope

The target is one descriptor matrix for every accessible mission control in:

- Add-on pages 11 through 15;
- the direct Mission CD bonus mission on selector page 4;
- Mission CD pages 16 through 19;
- New World and New World 2 composed pages 5/6;
- Original campaigns on page 20;
- Dark Tribe on page 21.

Selector 3 is navigation evidence. Selector 4 is navigation evidence except
for exact control 1919: the accepted executable proves that it dispatches
event `0x1B5F`, selects transition kind `0x0A`, and formats the exact fixed
relative `Map\\Campaign\\md_bonus.map`. Locked or unavailable content is
recorded as unavailable and is never unlocked, manufactured, or treated as a
failed mission.

Already accepted Phase 6B public snapshots, scroll/re-entry observations, and
Phase 6C.1 exact descriptor chains are reusable. Dark Tribe's complete static
catalog is reusable. No accepted family is retested merely because the catalog
is expanded.

The previous Phase 6D subset boundary is withdrawn. Phase 6D must not ship a
campaign-marker implementation that deliberately leaves an accessible family
for another repetitive marker phase. Before implementation, every accessible
mission is classified in one frozen matrix as either:

- `CLOSED`: exact control, dispatch ordinal, transition/formatter selection,
  exact relative, public page, and logical geometry are all proven; or
- `UNAVAILABLE`: the exact installed game exposes no selectable control for the
  content during the accepted full catalog sweep.

`EVIDENCE GAP` is allowed while research is in progress, but it blocks the
all-campaign implementation checkpoint. It is not converted to `CLOSED` from a
label, neighboring ordinal, filename pattern, or apparent menu order.

## Stage 1: one offline evidence closure

Perform one bounded offline analysis of the exact accepted executable and PDB,
using the already frozen identities:

- `S4_Main.exe` SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb` SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`.

For every mission control, close this chain independently:

```text
public page/control/rectangle
  -> exact constructor slot
  -> exact click-dispatch ordinal/event
  -> exact transition kind
  -> exact formatter slot or literal
  -> exact Map\Campaign\... relative
```

The analysis may group controls only when exact control-flow proves that they
share the same bounded dispatch and formatter implementation. A format string
proves possible relatives but does not by itself prove which public control
selects which ordinal.

Mine the frozen Phase 6B catalog logs for public controls, logical rectangles,
entry/scroll/top states, and composed-page ownership before requesting any new
live pass. Historical sparse observations may confirm geometry or page
residency; absence remains absence.

The page-4 bonus row is part of the matrix even though it resides on a selector.
No selector label, display value, or neighboring control may substitute for
its exact control/dispatch/formatter chain.

Produce one reviewable evidence artifact containing, for each mission:

```text
descriptor key | content group | page | control | rectangle | ordinal/event
transition kind | formatter/literal | exact relative | static window hashes
public evidence source | status
```

The artifact must explicitly supersede the historical incorrect `mcd2_*`
association where Phase 6C.1 proved `md_roman*`. Historical reports remain
unchanged as audit history.

## Stage 2: one gap-only catalog sweep, if needed

If offline analysis plus existing Phase 6B evidence leaves public geometry or
availability gaps, use one read-only diagnostic candidate and one continuous
game process. Do not deploy a sequence of family-specific probes.

The user performs one uninterrupted traversal of selector page 4, all campaign pages, and every
available scroll state, then reports completion once. No mission launch, hover,
victory, database access, save access, or per-page message is required. The
candidate writes only the existing project-owned diagnostic log.

The gap-only candidate samples the deterministic campaign owner from the
public screen API at GUI-element callback time, before the first UI-frame
callback can finalize the page epoch. It retains only exact statically proven
mission control IDs, so the initial full redraw is captured without hover while
navigation, decoration, and text remain non-identity data.

Do not run game processes concurrently. The installed archive, plugin log,
completion database, session IDs, and menu callbacks are shared; multiple
processes would make attribution ambiguous. Batching is achieved inside one
process, not by parallel game instances.

After the sweep, finish the remaining dispatch analysis offline and freeze the
single all-campaign matrix. If any accessible mission still has an evidence
gap, stop before marker implementation and report that specific chain rather
than beginning a partial campaign rollout.

## Strict identity and RD boundary

The only runtime identity authority remains confirmed same-MapInit-session
`identity.relative`. Static descriptor records constrain the expected identity
but never replace this confirmation for a live launch.

Never classify from `identity.name`, visible or translated text, a player save
name, database display fields, page ownership, filename stem, or an `RD` string
found anywhere except the confirmed relative identity. In particular,
`RD_PlayerSave` as a display/save value cannot make a fixed or campaign map
random. A true confirmed relative beginning with `RD_` remains random and can
never enter the campaign index.

## Campaign-session completion gate

The Phase 6C.1 Trojan acceptance exposed a necessary correction. Its exact
descriptor matched while the independent presentation origin was
`random-map/eligible`. Passing that origin directly into the older completion
pipeline would record the campaign mission as an ordinary single-player map.

A bounded `CampaignSessionAdmission` owns only:

- one nonzero active MapInit session ID;
- the exact matched immutable descriptor key;
- the descriptor's exact relative identifier;
- an admitted boolean.

It becomes admitted only after all of these are true in the same session:

1. an exact public page/control/rectangle armed a `CLOSED` descriptor lease;
2. the next fresh MapInit session bound that lease;
3. confirmed `identity.relative` arrived for that exact session;
4. `ValidateCampaignDescriptor` returned `Matched` for the same key and exact
   relative;
5. the independent origin was not explicitly online/excluded.

Only then may the completion pipeline receive
`{LaunchSource::Campaign, SessionEligibility::Eligible}` for that session. A
`Map\Campaign\` prefix or temporal proximity cannot admit completion. Session
change, descriptor mismatch, empty relative, online origin, malformed snapshot,
expiry, second click, page abandonment, or shutdown clears the gate.

Loaded campaigns without a fresh descriptor click retain the established
`LoadCampaign` origin path and may canonicalize to `Campaign` only through the
existing load-origin contract. An uncorrelated menu visit cannot classify a
loaded save.

## Campaign completion index

Add a separate `CampaignCompletionMarkerIndex` rather than weakening the
fixed-map `CompletionMarkerIndex` list-kind rules. Publication consumes an
owned `CompletionDatabaseSnapshot` and the frozen all-campaign descriptor
catalog.

A database record enters the campaign index only when:

- `mapKind == Fixed`;
- `launchSource == Campaign`;
- strict UTF-8 decoding succeeds within existing bounds;
- `stableId` exactly equals `BuildStableMapId(decoded relative)`;
- the decoded relative resolves to exactly one `CLOSED` descriptor;
- it exactly equals that descriptor's relative, including case and separators;
- no second valid record resolves to the same descriptor key.

The index publishes an immutable set of descriptor keys/relatives under one
mutex and answers bounded, allocation-free render queries. Invalid, duplicate,
ambiguous, online, random, wrong-source, unknown-descriptor, and inconsistent
records are excluded with one bounded summary diagnostic outside the draw loop.

The fixed-map index and its accepted RD behavior remain unchanged. No design or
test step authorizes reading the live database. Tests use constructed in-memory
snapshots only.

## All-campaign marker observer

Add one `CampaignMarkerObserver` that consumes successful owned
`CampaignMenuSnapshot` values, the frozen descriptor catalog, and the campaign
index. Each visible marker requires:

- exact snapshot page;
- exact control ID and rectangle;
- one unique `CLOSED` descriptor;
- one exact completed descriptor-key match;
- nonzero logical surface dimensions and checked geometry bounds.

Public text is used only to reject navigation/decorative controls; it is never
an identity or database key. A successful snapshot atomically replaces the
retained owner frame. Invalid, ambiguous, over-capacity, or non-campaign state
clears it. Hover/effects-only updates retain the same identity and geometry.

Size the neutral marker frame from the frozen matrix's maximum simultaneously
visible mission controls, with a compile-time bound no smaller than 16. Dark
Tribe's 12 controls must not truncate. `FixedMapRowObserver` remains limited to
its six accepted rows.

Reuse the accepted embedded PNG, `DirectDrawMarkerSurface`, and checked
`BuildMarkerCheckGeometry`. The marker remains anchored at each mission
button's right end, with pillarbox adjustment exactly once and clipping to the
button/destination surface.

On each public UI-frame callback:

1. compute the deterministic active campaign owner already used by capture;
2. finalize any changed public snapshot;
3. publish it to launch association and the campaign observer;
4. render commands only when the callback page equals that exact owner;
5. otherwise clear or skip the campaign frame;
6. preserve the fixed-map page-25 path unchanged.

There is one shared drawing surface and one renderer call per eligible frame.
The first stable page epoch displays without hover. Re-entry, effects-only
repaint, and scroll snapshots immediately replace and redraw the frame.

## One implementation candidate

After the evidence matrix is fully frozen, one implementation approval covers
the whole closed matrix. Do not merge or deploy family-specific intermediate
marker candidates. Keep intermediate RED commits local and push only the
complete GREEN checkpoint.

The production candidate restores only previously accepted components plus:

- the frozen all-campaign descriptor table and exact compatibility windows;
- the campaign-session completion gate;
- the campaign completion index;
- the all-campaign public observer and shared renderer scheduling.

Startup publishes an owned compatible store snapshot without writing it. An
unchanged database causes no normalization or commit. Campaign completion is
written only after a later explicitly authorized live victory reaches the exact
gate. Shutdown disables admission and observers, drains the bounded worker,
removes listeners in reverse order, and releases renderer/store objects without
leaving callback pointers to destroyed state.

## RED/GREEN and batch test contract

Tests must enumerate every `CLOSED` matrix row, not a hand-picked subset, and
prove:

- unique page/control/rectangle keys and exact relatives;
- compatibility admission requires all frozen windows for each family;
- control-to-relative validation rejects wrong ordinal, group, case, prefix,
  geometry, session, and historical `mcd2_*` guesses;
- exact same-session descriptor admission promotes only that session to
  Campaign, including the Trojan origin regression;
- explicit online origin and every mismatch never promote or write;
- index admission accepts only exact self-consistent Campaign records;
- `RD_PlayerSave` in every presentation field has no classification effect;
- a constructed in-memory completion snapshot containing every descriptor
  emits markers for every visible completed control across every layout and
  scroll state;
- non-completed, unavailable, duplicate, ambiguous, and malformed records do
  not emit markers;
- entry, repaint, scroll, re-entry, page transition, composed ownership, and a
  12-control Dark Tribe frame redraw without hover or truncation;
- pillarbox, scaling, clipping, overflow, PNG lifecycle, renderer failure,
  fixed-map markers, same-process index publication, and shutdown retain their
  existing GREEN regressions.

The all-completed in-memory fixture is test-only. It is never packaged,
activated in production, written to disk, or substituted for live database
evidence.

Complete MSVC Win32 CI, mutation fixtures, full tests, PE32/export verification,
package audit, and immutable artifact hashes remain mandatory before any
deployment request.

## Efficient future live acceptance

Implementation and artifact audit perform no live database access. Future live
acceptance requires separate explicit approvals for exact guarded deployment
and for a bounded database read/one-record campaign victory transaction.

Use one game process and one user report for the visual batch:

1. preflight exact archive/ASI/INI and, only after database-read approval,
   main/backup hashes, timestamps, record count, and temp siblings;
2. start at the main menu and prove compatible load causes no startup write;
3. traverse one representative of each distinct layout/ownership family:
   Add-on child, Mission CD child, composed New World 5/6, Original page 20,
   and scrolling Dark Tribe page 21;
4. exercise entry, one scroll where available, return, and re-entry without
   hover, then report the entire menu batch once;
5. use at most one explicitly authorized selectable campaign victory to prove
   the exact descriptor/session gate, one atomic record commit, same-process
   index publication, and immediate marker appearance;
6. close normally and perform one postflight audit.

The user does not launch every mission and does not win every campaign. Full
descriptor and layout coverage is authoritative in static analysis and the
all-row in-memory test matrix; the live pass verifies shared integration and
representative layouts. Existing exact completion records may display normally,
but no record is created merely to make a marker visible.

If the chosen mission already exists by exact stable ID, the transaction must
be `Duplicate` with no write. Selecting another mission requires renewed exact
live-write approval. No manual JSON edit, restore, save manipulation, campaign
unlock, or progress modification is implied.

## Completion boundary

Phase 6D GO means the exact accepted executable's accessible campaign mission
catalog is frozen as one evidence set and the common completion/marker contract
is GREEN across that set. It does not mean every mission has been completed by
the player, does not create completion records, and does not claim unavailable
content.

A future game executable change invalidates affected window hashes and requires
compatibility research. It must fail closed rather than infer identities from
names, ordering, neighboring missions, or path patterns.
