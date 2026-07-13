# Phase 2.4 Bounded Capture Trace Report

## Build and frozen artifact evidence

- Source commit: `5b32587736a8c8117b29bf707248b7caa549b2ec`.
- GitHub Actions run: `29244794329`, completed successfully for the source commit above.
- Successful jobs: `86799003794` (attempt 1) and `86799326844` (attempt 2).
- Artifact ID: `8276948848`; GitHub digest: `sha256:4be439d46a304545144e9a5936e97e8be2dbbe15fa5feabc7a9be6203a925cef`; size: `534925` bytes; not expired at verification time.
- Downloaded package SHA-256: `2c8acf3c56a1f5fc0e2e9e553478caf5eb5add9d137bf7ef2819e364ac6750fb`.
- Frozen ASI SHA-256: `8362d2487401252c852276534a960ad6a09534a593bc53945ff5289edea1d0c5`.
- Frozen INI SHA-256: `b35a23b6b1694261ec0c699f240687782572205da6bda03e6d5ee6b74aaccf4e`.

## Guarded deployment evidence

- Installed archive SHA-256: `c9aadd112aa7e5b7d102e5ccdd1686439e59120a0099215dda79cfc9bd5d1a6a`.
- The installed archive contained exactly one target ASI entry.
- Embedded ASI SHA-256: `8362d2487401252c852276534a960ad6a09534a593bc53945ff5289edea1d0c5`, equal to the frozen ASI.
- Immutable original archive SHA-256: `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`, unchanged and available for guarded restoration.
- Deployed INI SHA-256: `7d7300b498620d431faf389ce861bdb1ae1696bb761338bbfef2b605002aae3e`.
- A normalized textual diff proved that the deployed INI differs from the frozen INI only by changing the empty `CaptureTraceRoot` value to the approved project runtime directory.

## Live boundary evidence

- The user launched the Single list, explicitly clicked calibrated tab ID `2449`, selected Aeneas, entered the map, and remained in the running game.
- The live game process was PID `14156`; its exclusive trace was `capture-trace-14156.log`.
- Trace classification: `adapter-entered=0`; no wide-capture boundary was reached; no path-validation boundary was reached; `map-init-association=no-candidate`.
- A second exclusive file, `capture-trace-36324.log`, was empty and did not match the live game PID, so it was not used for the boundary decision.
- Controlled stop trace result: `controlled-stop-flush=success`.
- Hook stop result: `0`.
- Listener stop counts: registered `75`, removed `75`, failures `0`.
- The diagnostic runtime reported stopped. The process remained present with the operating-system responding flag set, and the user then confirmed normal in-game response.

## Scope statement

No completion, victory, persistence, or marker work occurred in this phase. These terms are present only to state the excluded scope and are not result claims.

## Conclusion

NO-GO: approved call site was not traversed by the fixed-map launch.

Per the approved plan, this task does not select or deploy another Hook. Any candidate change requires a new design and static-evidence review.
