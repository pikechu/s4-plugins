# Random-Map Recording and Marker Visibility Design

**Date:** 2026-07-14

**Status:** Approved in conversation; pending written-spec review

## Goal

Record valid local-player victories on random maps, including victories after
loading a save, while never showing the completed-level marker for a random
map in the map-selection UI.

## Requirements

- A random-map victory is recordable and may be persisted like any other
  eligible offline victory.
- Random maps never receive the completed-level marker in the UI.
- A loaded random map follows the same recording and visibility policy as a
  freshly generated random map.
- True online multiplayer remains excluded from victory recording.
- Single Player Maps, offline single-player Multiplayer Maps, Custom Maps,
  campaigns, tutorials, add-ons, and Mission CD maps remain recordable and
  marker-visible.
- Invalid, unavailable, stale, or conflicting map identity evidence must not
  be guessed into a map kind.
- No game executable call, process-memory probe, new hook, or save-file parser
  is introduced for random-map classification.

## Evidence

The paired live sample produced the same SU identity before and after loading:

```text
RD_LCGSDR30
```

The game implementation in `CRandomMaps::IsRandomMapFileName` classifies a
map identifier as random when it:

- starts with the case-sensitive prefix `RD_`;
- begins with `[` and ends with `]`; or
- begins with `<` and ends with `>`.

`CRandomMaps::GenerateRandomMapFileName` generates identifiers with `RD_`, and
the game uses `IsRandomMapFileName` when loading and validating maps. This is a
game-owned structural convention rather than a display-name heuristic.

## Considered Approaches

### 1. Mirror the game's pure identifier rule — selected

Apply the bounded `IsRandomMapFileName` rule to the validated SU relative map
identifier after map initialization. This covers fresh and loaded random maps
without calling undocumented game behavior.

Advantages:

- matches the game's own decision rule;
- works for ordinary save loads;
- adds no hook or binary-version dependency; and
- is directly unit-testable.

### 2. Parse the save container

Read and decompress the general save chunk to recover the stored map name.
This duplicates information already exposed by SU and adds substantial parser,
bounds-checking, and compatibility work.

### 3. Call or hook `CRandomMaps::IsRandomMapFileName`

Invoke the internal game function directly. This is unnecessary and would add
address admission, ABI, and executable-version risk.

## Architecture

### Independent policy axes

Map source, recording eligibility, and marker visibility are separate facts:

| Map kind | Record victory | Show completed marker |
| --- | --- | --- |
| Random map | Yes | No |
| Eligible fixed/campaign/offline map | Yes | Yes |
| True online multiplayer | No | No |
| Unresolved identity | No decision | No marker |

`ExcludedRandomMap` must no longer mean that a random-map victory is rejected.
Randomness is retained as map-source/type information, while recording policy
and marker policy consume that information independently.

### Classification input

Classification uses the validated value returned by
`SU.Game.GetMapNameRelativePath()`, because it is the game-relative identifier
rather than a localized display label. The validated display name remains
diagnostic corroboration but is not the classification authority.

The classifier exactly mirrors the game rule:

```text
random := starts_with("RD_")
       or (starts_with("[") and ends_with("]"))
       or (starts_with("<") and ends_with(">"))
```

The test is case-sensitive. Empty, invalid, absolute, traversal, stale,
conflicting, and unavailable values never reach classification.

### Data flow

1. `MapInit` consumes the menu/load origin and starts an identity session.
2. The SU bridge reads and validates map name and relative identifier.
3. The identity coordinator returns a bounded observation for the same session.
4. The random-map classifier refines the active map kind.
5. Online-multiplayer exclusion retains precedence over all refinements.
6. Diagnostics record the initial origin and the post-identity refinement.
7. Future persistence code records a random-map victory.
8. Future marker rendering checks marker visibility and suppresses random maps.

This phase implements classification and the separated policy model. It does
not enable completion persistence or marker rendering before their respective
phases are implemented and approved.

## Failure Handling

- Missing or invalid SU identity leaves the map kind unresolved.
- A result whose session ID does not match the active `MapInit` session is
  ignored.
- Online multiplayer can never be promoted to recordable by identity
  refinement.
- Diagnostic failures do not call into or modify the game.
- Existing fail-closed behavior remains for unresolved identity.

## Testing

Automated tests must cover:

- `RD_` identifiers and both bracket forms;
- case sensitivity and malformed brackets;
- normal fixed/custom relative paths;
- the observed `RD_LCGSDR30` fresh/load identifier;
- random-map recording eligibility and hidden marker visibility;
- fixed/campaign eligibility and visible markers;
- online multiplayer exclusion precedence;
- invalid, stale, conflicting, and mismatched-session identity results; and
- bounded refinement trace output.

The complete Windows x86 test/build suite and repository policy checks must
pass before any deployment is proposed. Deployment remains blocked while the
game process is running and requires the existing explicit approval flow.
