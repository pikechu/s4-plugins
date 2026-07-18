# Phase 5B internal-menu relative-marker rendering report

Date: 2026-07-15 (Asia/Shanghai)

## Decision

GO for the read-only internal-menu relative-identifier rendering adapter.

The completed-map marker is present on first entry without a row hover, remains
synchronized with the internal menu during scrolling, is isolated by fixed-map
list kind, and appears automatically for the completed custom map. The adapter
does not create identity, classify RD names, write game memory, write Lua state,
or modify completion persistence.

The completion store remained in its existing read-only-backup state. This GO
does not repair or authorize mutation of the invalid main database.

## Reviewed candidate

- source commit: `ecffd6f0fb24fa917f1df35b172971443b4efa72`
- GitHub Actions run: `29412025953`
- job: `87341114122`
- artifact: `8341546183`
- authoritative result: success
- artifact SHA-256:
  `8608b79fdeb989ba54456a9469641cb51c8b7ac62cfb7217ae7d341aadd7ccff`
- ASI SHA-256:
  `4ac06ab5abd0b6655389563c27a6763d5cbef8bdaebbed768ee1db7240525643`
- packaged INI SHA-256:
  `755a2aea72fd3c6ae840d3116b9a2f0d2f9fbbf2d2a36cf788a583469bdf8b9d`

Windows CI passed MSVC Win32 compilation, policy checks, all forbidden-behavior
mutation fixtures, unit tests, packaging, PE32 verification, export inspection,
and exact package-layout verification.

## Deployment audit

The user explicitly approved deployment of the audited Phase 5B internal-menu
relative-identifier rendering candidate. Protected processes were closed before
deployment.

- previous `Plugin_SU.zip` SHA-256:
  `d76fcf90bbecb783af70d8051e3e38d07dfc92f25997910bd301e96930c14bf8`
- installed `Plugin_SU.zip` SHA-256:
  `906b65605491ea2e147b7c3fb11568abd07aa76ff2868ed2f3e5fa37f4f057d4`
- immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`
- embedded ASI SHA-256:
  `4ac06ab5abd0b6655389563c27a6763d5cbef8bdaebbed768ee1db7240525643`
- installed INI SHA-256:
  `755a2aea72fd3c6ae840d3116b9a2f0d2f9fbbf2d2a36cf788a583469bdf8b9d`
- unchanged main database SHA-256:
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`
- unchanged backup database SHA-256:
  `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`

The guarded installer preserved the immutable original archive, verified the
embedded ASI, left no archive or INI temporary sibling, and recorded rollback
copies under the ignored run artifact directory.

## Live evidence

The deployed runtime logged:

- `bootstrap version=0.6.2 ... mode=internal-menu-relative-marker-rendering`;
- the exact ASI SHA-256 shown above;
- `internal-menu-adapter=admitted-read-only`;
- `completion-store mode=read-only-backup records=2`;
- stable fixed-map page set `{4,22,23,25}`;
- no internal-menu read failure during the acceptance sequence.

The user-driven acceptance sequence produced these results:

1. Single Player Maps opened with the cursor not hovering a row. The Aeneas
   marker appeared by default immediately, as expected.
2. At `scroll=0`, slot 0 was
   `Map\Singleplayer\Aeneas.map`. Scrolling down produced `scroll=1` with
   Aeneas outside the six-row snapshot. Returning to the top produced
   `scroll=0` again, and the marker reappeared immediately without hover.
3. Multiplayer Maps exposed only `Map\Multiplayer\...` identifiers. The user
   confirmed that no marker appeared, matching the absence of a completed
   record for that list.
4. Custom Maps exposed `Map\User\Antares.map` in the visible snapshot. The
   Antares marker appeared automatically without hover, as expected.
5. Returning to the main menu changed the observed pages back to page 1,
   removing the exact fixed-map page gate. The user then exited normally.

The captured log is stored at:

`artifacts/phase-5b-internal-menu-relative-rendering/run-29412025953/live/CampaignCompletion.log`

Its SHA-256 is:

`c938b0f23a66064dfd785e459439215abbd9a06d2a2546bb01e63e56065ad609`

After exit, `S4_Main`, Settlers United, and `SettlersUnited` were absent. The
installed archive, embedded ASI, INI, main database, and backup database hashes
still matched the deployment audit, and no stop or deployment temporary file
remained.

## Boundary conclusion

Menu values are used only to query the already validated fixed-map completion
index by relative identifier and admitted list kind. Completion classification
continues to use the confirmed, session-bound `identity.relative`; a player save
or display name such as `RD_PlayerSave` cannot classify a fixed map as random.
A true `RD_...` relative identifier remains outside the fixed-list marker index.
