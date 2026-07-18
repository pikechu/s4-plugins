# Phase 6E campaign session admission and one-victory persistence design

Date: 2026-07-18 (Asia/Shanghai)

## Objective and current authorization

Phase 6E connects the already accepted exact campaign menu/session association
to the established native local-victory and atomic completion-store pipeline.
One future exact campaign victory may then create one `Campaign` completion
record and publish both marker indexes in the same process.

The current authorization covers design, offline implementation, constructed
tests, Windows CI, and artifact audit. It does not authorize deployment,
starting or controlling the game, reading or writing the live database, save
access, campaign-progress access, or a live victory. Deployment and the exact
one-record live transaction require fresh explicit approvals after artifact
audit.

## Reused authority

Phase 6E does not invent another victory or storage mechanism. It restores the
previously accepted components:

- exact native event admission and event `609` subscriber;
- `VictoryEventProbe` local-result and duplicate/conflict handling;
- `CompletionCandidateCoordinator`;
- `CompletionAdmission`;
- bounded `CompletionWorker`;
- atomic `CompletionStore`;
- fixed-map and campaign completion indexes;
- the Phase 6C.1 exact click, MapInit, identity-relative, and immutable
  descriptor association;
- the Phase 6D 107-row descriptor catalog and marker observer.

No Hook, code patch, synthetic input, game call, Lua write, game-data write,
save write, campaign-progress write, display-name fallback, or path-prefix-only
campaign classification is added.

## Bounded campaign session gate

Add a `CampaignSessionAdmission` that owns only:

- one nonzero active MapInit session ID;
- the MapInit presentation origin;
- an enabled boolean.

`BeginSession` replaces all prior state. `Disable` clears it permanently. The
gate returns a promoted `{Campaign, Eligible}` origin only when one observation
proves all of the following:

1. `CampaignLaunchAssociation` bound an exact admitted public
   page/control/rectangle click to the same fresh MapInit session;
2. confirmed `identity.relative` belongs to that same session;
3. `ValidateCampaignDescriptor` returned `Matched`;
4. the validation record key exactly equals the armed descriptor key;
5. the association relative, confirmed identity relative, and immutable
   descriptor relative are byte-for-byte equal, including case and separators;
6. the original MapInit origin was not explicitly online or otherwise
   ineligible.

The gate is one-shot for its session. Any zero/mismatched session, missing
record/key, wrong relative, wrong geometry result, explicit online origin,
second observation, new session, or shutdown fails closed. It never examines
`identity.name`, visible text, save/display names, a `Map\Campaign\` prefix, or
an `RD` string outside confirmed relative identity.

Loaded campaigns without a fresh menu click continue through the established
`LoadCampaign` canonicalization contract. An ordinary uncorrelated menu visit
cannot promote a loaded save.

## Runtime order

On MapInit:

1. consume the independent presentation origin;
2. create the new identity session;
3. bind any exact pending campaign click;
4. begin `CampaignSessionAdmission` with the same session and independent
   origin;
5. begin `CompletionAdmission` and `VictoryEventProbe`.

On confirmed identity:

1. require the exact active session;
2. obtain the one-shot campaign association, if any;
3. validate it against the immutable descriptor catalog;
4. ask `CampaignSessionAdmission` for an exact promotion;
5. otherwise apply only the established origin refinement;
6. pass the identity and final origin to `CompletionAdmission`.

On native event `609`, the existing probe admits only a local win for the same
session. Identity and victory may arrive in either order. The coordinator emits
at most one candidate.

## Store and publication contract

Startup uses `CompletionStore::Load` and publishes its owned snapshot to both
indexes. The exact current compatible main database must load
`WritableLoaded` without normalization or a startup write. Missing or
incompatible states retain the existing store's fail-closed modes; no
campaign transaction is attempted unless the worker is writable.

The worker writes only after the exact session gate and local-victory chain
emit a candidate. `CompletionStore::AddIfAbsent` preserves the established
temporary-write, flush, re-read, validate, backup, and atomic replacement
transaction.

After `Committed`, one owned store snapshot is published to both:

- `CompletionMarkerIndex`;
- `CampaignCompletionMarkerIndex` with the immutable admitted catalog.

`Duplicate`, `ReadOnly`, and `Failed` never publish a synthetic update. A
committed campaign marker becomes available in the same process when the user
returns to its menu page; no restart or hover is required.

## Shutdown

Controlled shutdown performs this order:

1. disable completion and campaign-session admissions;
2. disable marker observers and renderer;
3. request native subscriber detach and service it on the game thread within
   the established bounded timeout;
4. close the callback gate and remove public listeners in reverse order;
5. drain the worker within five seconds;
6. release admission, worker, store, renderer, and registration objects.

Failure to detach or drain is logged and fails the postflight; it never
terminates the game process.

## RED/GREEN contract

Constructed tests must prove:

- Trojan's previously observed misleading random presentation origin promotes
  only after exact same-session descriptor validation;
- Mission CD, New World, Original, and Dark Tribe representatives use the same
  gate;
- wrong session, key, case, separator, prefix, geometry status, empty
  relative, second observation, explicit online origin, and true `RD_`
  relative never promote or submit;
- `RD_PlayerSave` in identity/display fields has no effect;
- victory-before-identity and identity-before-victory each commit at most once;
- loss, malformed event, duplicate/conflicting terminal events, and
  uncorrelated campaign visits never write;
- a committed Campaign record publishes both indexes in the same process;
- duplicate and failed transactions do not publish;
- startup of a compatible current snapshot performs no write;
- shutdown ordering and all existing fixed-map, renderer, persistence, and
  mutation regressions remain GREEN.

Intermediate RED commits remain local. Push only a complete GREEN checkpoint.

## Efficient live acceptance

After separate guarded deployment and exact live database transaction approval,
use one game process and one selectable campaign mission:

1. freeze archive, ASI, INI, database main/backup hashes, timestamps, record
   count, and temporary siblings;
2. start at the main menu and prove startup caused no database write;
3. select the predeclared exact mission and allow its normal automatic or
   user-driven victory;
4. observe exactly one local event `609`, one exact Campaign candidate, and
   either one `Committed` transaction or a predeclared `Duplicate`;
5. return to the mission page and verify the marker appears immediately
   without hover;
6. close normally and perform one postflight.

No family-by-family wins are required. Static all-row tests cover all 107
descriptors; the single live victory validates the shared integration.

## Completion boundary

Phase 6E GO means one exact campaign session can be recorded and rendered by
the common pipeline without weakening classification or storage guarantees.
It does not mark all campaigns complete, modify game progress, or authorize
additional victories. Every later live transaction remains bounded by the
database state and explicit user approval in force at that time.
