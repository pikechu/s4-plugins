# Phase 6C.1 descriptor family and session-gate correction design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Correct the two independently observed Phase 6C failures without expanding the
read-only diagnostic boundary:

1. re-freeze the page-16 control `1903` chain after the authoritative session-2
   result rejected `mcd2_roman1` and confirmed `md_roman1`;
2. bind a known exact descriptor click to MapInit without depending on the
   separate presentation-sensitive `LaunchOrigin` tracker.

This design does not authorize implementation or deployment. It does not
authorize marker rendering, completion reads or writes, database access, save
access, campaign-progress changes, internal calls, mutable private state,
patches, hooks, synthetic input, or process termination.

## Frozen live facts

- Add-on public click: page 11, control `91`, rectangle
  `320,200,175,30`.
- Session 1 confirmed exact relative:
  `Map\Campaign\ao_trojan01.map`.
- No `campaign-menu-association session-armed=1` or descriptor validation was
  emitted. Temporal adjacency is not accepted as an anchor.
- Page-16 public click: control `1903`, rectangle `237,148,175,30`.
- Session 2 emitted one exact association and confirmed
  `Map\Campaign\md_roman1.map`.
- The immutable record expected `Map\Campaign\mcd2_roman1.map` and correctly
  rejected it as `relative-mismatch`.
- Original page 20 control `2039` matched exact
  `Map\Campaign\viking02.map` in session 3.
- Database and backup were byte- and timestamp-identical after shutdown.

`identity.relative` is the only authority. `identity.name`, translated labels,
S4ModApi enum names, save names, and strings containing `RD` remain excluded.

## Offline path-chain correction

Disable the complete current Mission CD descriptor group before analysis. Do
not replace `mcd2_` with `md_` by string substitution and do not promote the
page-16 live result into sibling controls.

Re-read the exact executable and PDB chain for page 16 control `1903`:

1. public control construction and stable rectangle;
2. bounded click normalization and jump-table branch;
3. exact zero-based ordinal;
4. internal transition state selected by that branch;
5. formatter-table base and exact slot reached by that state;
6. exact UTF-16 format literal initialized in that slot;
7. exact result `Map\Campaign\md_roman1.map`.

Freeze every selected RVA, file offset, length, bytes, and SHA-256. Only records
whose complete chain reaches the same proven formatter family may be restored.
Page 16 control `1904`, page 18 control `1889`, and all currently compiled
siblings remain disabled unless their own dispatch-to-slot chain is closed.

The group label in source should describe the proven transition family, not
assume the public enum name is semantic identity. Renaming a group is allowed
only as documentation; selection continues to use page/control/geometry and
exact relative identifiers.

## Known-descriptor click lease

The existing `CampaignLaunchAssociation` observes an exact public snapshot and
arms only one labeled control, but `BeginSession` additionally requires
`LaunchOrigin::Campaign`. The Add-on run shows that independent page-composition
tracking can reject a valid known campaign click before the authoritative
identity arrives.

Phase 6C.1 should split validation into two stages:

```text
public descriptor lookup:
  admitted group + exact page + exact control + exact rectangle

same-session descriptor validation:
  pending known descriptor + fresh MapInit session + exact identity.relative
```

Only a public click that uniquely resolves to an already admitted immutable
descriptor may create the 30-second pending lease. `BeginSession` then binds the
next nonzero MapInit session without requiring a positive `LaunchOrigin`
classification. An explicitly online/excluded origin still rejects and clears
the lease. Unknown origin is logged but is not stronger than the exact admitted
campaign control.

The later same-session exact-relative comparison remains mandatory. Backing out
and launching an unrelated map therefore produces `relative-mismatch`, never a
successful descriptor. Page change to a different campaign catalog, lease
expiry, malformed snapshots, multiple control matches, a second click, session
mismatch, empty relative, shutdown, and explicit online origin all clear the
pending state.

The session-armed diagnostic must log the pending descriptor key plus the
independent origin source and eligibility. It must not log or inspect display
names.

## RED tests

Add or update tests proving:

- only an admitted exact page/control/rectangle can arm a lease;
- a geometry mismatch, unknown control, disabled group, or New World evidence
  gap cannot arm;
- an unknown origin can bind the next fresh session for a known descriptor;
- an explicitly online origin cannot bind;
- a different campaign page clears the lease;
- expiry and session mismatch fail closed;
- Add-on `91 -> ao_trojan01` matches through the full association flow;
- page 16 control `1903` accepts only the newly frozen exact relative;
- `mcd2_roman1`, a wrong ordinal, case/prefix variation, and a display value
  beginning with `RD` cannot change the result;
- Original `2039 -> viking02` remains GREEN;
- completion store, marker, native event, internal menu, Hook, patch, Lua write,
  and game-data-write paths remain absent from the runtime.

## Efficient live acceptance

After separate implementation approval, complete Windows CI, artifact audit,
normal shutdown, and a fresh exact deployment approval, one process is enough:

1. repeat Add-on Trojan 1 and require one session-armed record plus exact
   `ao_trojan01` match;
2. repeat only page 16 control `1903` and require the re-frozen exact match;
3. do not repeat Original or Dark Tribe unless shared association code changes
   invalidated their accepted anchors.

Because the association gate will change, Original and Dark remain static
regressions in tests; the live batch stays limited to the two failed paths.
Normal-shutdown postflight must again preserve database main/backup bytes and
timestamps and leave no temporary siblings.

## Acceptance boundary

Phase 6C.1 becomes GO only if both failed paths emit exactly one fresh,
monotonically increasing, same-session descriptor match and no rejection. A
failure remains local and fail-closed. Even full Phase 6C.1 GO authorizes only
the immutable descriptor contract. Campaign marker indexing and rendering
remain Phase 6D and require separate design and approval.
