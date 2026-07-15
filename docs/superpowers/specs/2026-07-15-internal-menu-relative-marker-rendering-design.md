# Internal Menu Relative-Identifier Marker Rendering Design

Date: 2026-07-15 (Asia/Shanghai)

## Decision

Use the GO read-only fixed-map menu snapshot to produce completion-marker row
commands without waiting for a public GUI text callback. The internal value is
used only as a lookup key into the already validated completion-marker index.
It never becomes session identity and never creates a completion record.

No process-memory write, code patch, Hook, input synthesis, cursor movement,
game-state mutation, Lua write, or database repair is added.

## Index contract

`CompletionMarkerIndex` retains the validated relative identifier alongside
the display name for each admitted fixed-map completion record. A new bounded,
allocation-free `MatchRelative` query compares a menu-relative identifier with
that retained value using ordinal case-insensitive path comparison and `/` to
`\` separator equivalence.

The query also requires the already admitted fixed-list kind. A wrong list,
random record, online source, malformed record, missing match, or duplicate
match returns no drawable completion. Display text and player save names are
not consulted.

## Full-frame snapshot

On a successful internal snapshot of up to six visible rows,
`FixedMapRowObserver` atomically replaces its retained marker slots:

- slot `n` uses the calibrated logical rectangle at x `115`,
  y `142 + 31*n`, width `271`, height `30`;
- a command is retained only when `MatchRelative(current_list_kind,
  relative_identifier)` returns `Unique`;
- incomplete rows are explicitly cleared;
- a failed or malformed internal snapshot clears the entire internal frame;
- once an internal snapshot is active for the exact page set, public GUI text
  observations cannot mix into that frame.

If the executable or instruction admission gate rejects the internal adapter,
the existing public row-observation path remains available unchanged.

## Immediate page gate

The existing one-second `PageObservationWindow` remains for bounded diagnostic
logging. Rendering no longer waits for it.

The listener separately accumulates the public UI page callbacks that occur
between page-25 callbacks. Page 25 closes a render cycle. The first transition
cycle may contain stale pages and fails closed; the next cycle must contain
exactly `{4,22,23,25}`. This normally admits the internal snapshot within one
steady render cycle rather than after one second.

Any extra or missing page clears the retained frame. The internal snapshot is
read before the final page-25 render call, so a scroll or tab model change can
replace the six slots in the same accepted cycle.

## RD and identity boundary

Menu-relative identifiers have the same textual shape as confirmed
`identity.relative`, but they are presentation-model evidence only. Random-map
classification and persistence continue to use the confirmed, session-bound
identity result. A save/display name such as `RD_PlayerSave` cannot affect
classification or marker lookup. A true `RD_...` relative identifier is not an
admitted fixed-list completion candidate and remains unmarked.

## Acceptance

Unit and policy tests must prove:

- exact relative matching across case and slash variants;
- wrong-list, random, malformed, and ambiguous values fail closed;
- an initial Aeneas snapshot draws slot 0 without any GUI element callback;
- scrolling moves the marker to the new slot and clears the old slot;
- tab changes use only that list's relative identifiers;
- read failure and page departure clear all internal commands;
- public observations cannot contaminate an active internal frame;
- the fast page-cycle gate rejects transition pages and admits only the exact
  steady set;
- no internal snapshot is connected to identity creation or persistence.

The resulting candidate requires complete Windows CI, artifact audit, fresh
deployment approval, and another user-driven live check before it can replace
the diagnostic deployment.
