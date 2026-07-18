# Phase 6A Dark Tribe campaign-menu forensics implementation plan

Date: 2026-07-16 (Asia/Shanghai)

## Goal

Build and validate a public-S4ModApi-only, structurally read-only diagnostic
that captures the Dark Tribe mission menu and associates one unique user launch
with its confirmed session-relative map identity.

The governing design is
`docs/superpowers/specs/2026-07-16-phase-6a-dark-tribe-campaign-menu-forensics-design.md`.

## Task 1: Frozen evidence and RED contracts

- Add a normalized static-evidence report containing the approved EXE/PDB
  identities, page-21 enum, campaign PDB records, historical page-21 excerpt,
  and reviewed disassembly references.
- Add RED unit tests for bounded campaign feature capture, page epochs,
  conflict/overflow rejection, click association, session mismatch, and the RD
  archive-name rule.
- Add runtime policy tests requiring structurally disabled completion writes,
  native terminal subscription, and all marker rendering in Phase 6A mode.
- Keep the RED checkpoint local; do not push or run full CI for incomplete
  production code.

## Task 2: Bounded public campaign capture

- Add `CampaignMenuCapture` with fixed-capacity project-owned snapshots.
- Route only the minimum public page/frame, GUI-element, and mouse observations
  needed by the component.
- Reject ambiguous page ownership, callback ordering, malformed text,
  collisions, concurrent epochs, and capacity overflow.
- Emit a bounded log only when the accepted snapshot changes.
- Run native tests locally and preserve all fixed-map regression behavior.

## Task 3: Session-confirmed launch association

- Add `CampaignLaunchAssociation` with one pending unique control.
- Bind click, campaign launch origin, MapInit session, and confirmed SU relative
  identity without retaining any game pointer.
- Log accepted and rejected association status without creating a completion
  record or marker command.
- Require exact session matching and clear state on page exit, back, timeout,
  conflict, or shutdown.

## Task 4: Enforced read-only runtime candidate

- Add a dedicated Phase 6A diagnostic mode and INI policy.
- In that mode, do not construct the completion worker/admission, native victory
  subscription, fixed-map observer/renderer, or any campaign renderer.
- Preserve compatible executable admission, public listeners, SU Lua identity,
  callback quiescence, listener removal, and normal shutdown.
- Give optional runtime dependencies explicit null/disabled handling rather
  than weakening production-mode requirements.
- Keep the persistence writer version `0.6.0`; use a separate diagnostic
  bootstrap version.

## Task 5: GREEN checkpoint and artifact audit

- Run formatting/static checks and all available local tests.
- Commit and push one complete GREEN checkpoint to `main`.
- Require authoritative Windows Release CI: configure/build, `/W4 /WX`, all
  tests, policy/mutation gates, PE32/i386 verification, exact two-entry package,
  and artifact upload.
- Download and independently audit the successful artifact and its embedded
  ASI/INI. Do not deploy an intermediate or unaudited build.

## Task 6: Guarded live forensics

- Obtain fresh user approval only after the game and Settlers United are
  closed.
- Freeze installed archive, ASI, INI, four-record main JSON, three-record
  backup JSON, and absence of temporary siblings.
- Deploy only through the guarded archive procedure.
- Ask the user to perform the design's one-action-at-a-time Dark Tribe sequence.
- Read logs and project JSON only; never navigate, close, or terminate the game
  for the user.
- Freeze postflight evidence and issue exactly one GO or evidence-gap decision.

## Completion boundary

This plan ends with a verified page-21 feature snapshot and one exact
control-to-relative-identity association, or a bounded evidence gap. It does
not implement or authorize campaign markers, manual completion, hot reload,
backup restoration, JSON editing, or a release package.
