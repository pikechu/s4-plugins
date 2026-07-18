# Phase 2.5 Settlers United Lua Map Identity Report

## Build and frozen artifact evidence

- Source commit: `af4c825282fcc1f1d839a25ca5b22daa7ccb860a`.
- GitHub Actions run: `29249674269`.
- Successful jobs: `86814880789` (attempt 1) and `86815268111` (attempt 2).
- Both attempts passed the Win32 build, zero-process-patch verifier, both mutation-rejection fixtures, CTest, PE32/package checks, and artifact upload.
- Artifact ID: `8278901499`; GitHub digest: `sha256:4a8ccea1e9f1371c38fe090c275916cc2250d69b5f96e06dd36868d71d224874`; downloaded artifact size: `521082` bytes.
- Downloaded artifact SHA-256: `4a8ccea1e9f1371c38fe090c275916cc2250d69b5f96e06dd36868d71d224874`, equal to the GitHub digest.
- Inner diagnostic package SHA-256: `fb3dbd4e678ae325dce83e0fd77534010349be4370aa10c5ad7ef02767007b71`; size: `521422` bytes.
- Frozen ASI SHA-256: `8cccf1e375ab22318dae729f75e760e90e3f49109547788e89a2dfb49e7331a7`; size: `1587712` bytes; format: PE32 `pei-i386`.
- Frozen INI SHA-256: `7b1f55fc852ed940f66724be14192e3607984d894f60ecb6784b94a430a86e61`; size: `430` bytes.
- The inner package contained exactly `Plugins/CampaignCompletionDebug.asi` and `CampaignCompletion/CampaignCompletionDebug.ini`.
- The frozen INI identified version `0.2.5`, `IdentitySource=SettlersUnitedLua`, `HookCount=0`, `CodePatchBytes=0`, and `LuaWrites=0`; its packaged `CaptureTraceRoot` was empty.

## Guarded deployment evidence

- The guarded installer accepted the previously recorded installed archive and produced installed archive SHA-256 `ae885f3085ef1ba9fc0d17fa22609256007fe7cda14d448e93396b78b2f3759b`; size: `1676703` bytes.
- The installed archive contained exactly one `Plugins/CampaignCompletionDebug.asi` target entry.
- Embedded ASI SHA-256: `8cccf1e375ab22318dae729f75e760e90e3f49109547788e89a2dfb49e7331a7`, equal to the frozen ASI.
- Immutable original archive SHA-256: `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`, unchanged and available for guarded restoration.
- Deployed INI SHA-256: `3da978ee4311a9055727a1e7be78f5973b5f1a03a0fc2737ef7415b5ef9a9049`.
- A normalized textual diff proved that the deployed INI differed from the frozen INI only by replacing the empty `CaptureTraceRoot` with the approved Phase 2.5 project runtime directory.
- Read-only process checks found no game or Settlers United process before archive and INI deployment.

## Live identity evidence

- The user launched normally, entered Single Player Maps, clicked the Single Player Maps tab, selected Aeneas, entered the map, and remained there for capture.
- The live game process was PID `5640`; its exclusive project trace was `capture-trace-5640.log`.
- Map-init session: `1`; explicit list attribution: `single`.
- Lua-open generation: `1`. MapInit occurred before the first observed LuaOpen, and the pending session bound once to that generation as designed.
- SU map-name status: `success`; validated display name: `Aeneas`.
- SU relative-path status: `success`; validated stable relative key: `Map\Singleplayer\Aeneas.map`.
- Both values belonged to the same session and Lua generation; terminal association: `confirmed`.
- The trace contained no pointer, Lua object, Lua stack data, arbitrary memory content, or absolute map path.

## Controlled-stop and responsiveness evidence

- Only `CampaignCompletionDebug.stop` was created for controlled shutdown, and the runtime consumed it.
- Listener stop counts: registered `77`, removed `77`, failures `0`.
- The runtime reported `diagnostic runtime stopped`.
- The exclusive trace ended with `controlled-stop-flush=success`.
- `S4_Main.exe` PID `5640` remained running after plugin shutdown.
- The user exercised the running game and confirmed normal response.

## Scope statement

This phase established fixed-map identity only. It did not implement victory observation, completion detection or persistence, or completion markers.

## Conclusion

GO: the explicit list epoch, MapInit, SU map name, SU relative path, and Lua generation agreed.
