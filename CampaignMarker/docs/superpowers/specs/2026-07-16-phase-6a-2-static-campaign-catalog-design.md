# Phase 6A.2 static Dark Tribe catalog design

Date: 2026-07-16 (Asia/Shanghai)

## Objective

Provide a complete, deterministic 12-mission Dark Tribe catalog without
reading a private mutable menu structure. Close the no-hover catalog evidence
gap while preserving the Phase 6A.1 public click-to-session association and
all zero-write boundaries.

This design does not authorize implementation, deployment, campaign marker
rendering, victory handling, persistence, or database repair.

## Chosen approach

Use an immutable compile-time table of 12 records:

```text
ordinal, public control ID, logical 800x600 rectangle, relative identifier
```

The table is justified by the exact executable's consecutive control
construction, exact control-to-zero-based-task dispatch, Dark Tribe path
format literal, the complete Phase 6A.1 public geometry snapshot, and the
same-session Mission 3 dynamic anchor.

Do not build a private menu reader. No mutable game pointer, campaign-progress
value, label pointer, or game-owned collection is needed.

## Compatibility admission

`AdmitDarkTribeCatalog` will be a read-only startup check. It accepts only the
already approved `S4_Main.exe` identity and the reviewed immutable code/literal
windows. It copies no private state and calls no game function. Failure returns
one disabled catalog; partial admission is forbidden.

The runtime exposes the table only while public page 21 is active. Leaving the
page clears the published view but does not mutate the immutable table.

## Public cross-check

Sparse public GUI features remain useful as an independent validator. For any
observed mission control `2081..2092`, ID and rectangle must equal the static
record. Label presence may prove that the element is a mission-like control,
but label content is never identity. `effects` remains volatile and is not
part of catalog identity.

An ID/rectangle disagreement, duplicate static ID, unexpected mission ID,
incompatible executable, unreadable immutable window, page disagreement, or
shutdown disables the catalog for that visit and emits only a bounded
diagnostic. It never falls back to label parsing or an adjacent session.

## Association and RD boundary

Phase 6A.1 continues to own live launch association. One unique exact public
click arms the bounded lease; only the same-session confirmed
`identity.relative` may validate a catalog record.

Neither `identity.name`, a save filename, nor visible menu text may create or
change a mapping. A player-selected name such as `RD_PlayerSave` is irrelevant.
For a campaign session, only a confirmed relative such as
`Map\Campaign\dark03.map` is compared to the static record. A wrong session,
empty relative, non-campaign origin, or mismatching relative fails closed.

## Diagnostic runtime boundary

The first Phase 6A.2 implementation candidate remains structurally read-only:

- no completion store, worker, admission, or JSON write;
- no native victory subscription;
- no marker index or renderer;
- no private mutable menu reader or internal function call;
- no process patch, hook, synthetic input, Lua write, campaign-progress edit,
  database restoration, or save modification;
- diagnostics only in the project-owned log directory.

Its only new output is one deterministic catalog record and public cross-check
status on page 21, plus the existing Phase 6A.1 association records.

## Efficient batch acceptance

Do not run multiple game processes concurrently. They share the SU archive,
plugin log, and database, so concurrent runs would make session attribution
ambiguous and increase write/rollback risk.

Use one audited deployment and one game launch for the whole batch:

1. Freeze process count, archive/INI identities, database bytes/timestamps,
   and the log start offset once.
2. Enter Dark Tribe once and require one complete static catalog plus public
   cross-checks for whichever sparse controls appear.
3. Sequentially launch currently selectable Missions 1, 3, and 6. After each
   map loads, return normally to the Dark Tribe page and select the next. No
   game/SU restart and no per-navigation Codex acknowledgment is required.
4. Each launch must receive a new monotonically increasing session ID and one
   exact association:
   `2081 -> dark01`, `2083 -> dark03`, `2086 -> dark06`.
5. After the third launch, return to the main menu, close normally, and report
   once. Freeze the log and perform one postflight database/process/temp check.

If automatic AI Resign occurs, it is accepted as existing game behavior; the
user need not suppress it. Missions 7 through 12 are currently locked and are
not bypassed. Their mappings rely on the admitted static evidence, not on save
or progress edits.

## Acceptance decision

Phase 6A.2 diagnostic GO requires:

- exact compatibility admission and all 12 deterministic catalog records;
- no static/public geometry disagreement;
- the three selectable batch associations above, each exact and single-use;
- no cross-session promotion and no display/save-name classification;
- byte-identical database main and backup files, unchanged timestamps, zero
  protected processes after exit, and no temporary siblings.

Even a diagnostic GO authorizes no campaign marker. Marker indexing and
rendering require a separate design and explicit approval after this catalog
contract is live-validated.
