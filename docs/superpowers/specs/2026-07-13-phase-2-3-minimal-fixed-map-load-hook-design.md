# Phase 2.3 Minimal Fixed-Map Load Hook Design

Date: 2026-07-13
Target: The Settlers IV History Edition through Settlers United
Branch: `main`

## Goal

Phase 2.3 obtains a stable loaded-map identity for fixed maps by observing one
reviewed internal call site. It covers one launched map from each of:

- Single Player Maps;
- Multiplayer Maps;
- Custom Maps.

The phase does not cover campaigns, random maps, saved games, multiplayer
session classification, victory detection, completion storage, completion
markers, or user-facing marker rendering.

## Authorization and Mutation Boundary

The user approved evaluating and implementing one minimal internal Hook for map
identity. Installing that Hook temporarily replaces one five-byte `CALL` in the
running `S4_Main.exe` image. The patch is process-local and must be restored
before diagnostic shutdown.

No game or Settlers United disk file may be modified except the previously
authorized project-owned `CampaignCompletion*.asi`, the `CampaignCompletion`
configuration directory, and guarded replacement of
`C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
The verified original archive backup remains immutable.

The plugin may not add another internal Hook, scan the heap, invoke an unrelated
game function, modify a map or save, write completion data, or install a Hook
when executable identity or target bytes are uncertain.

## Selected Approach

The selected observation point is the fixed-map wrapper call at VMA
`0x004FEFA5`, RVA `0x000FEFA5`, in the approved executable. Its reviewed bytes
are:

```text
004FEF97  6a 01                 push 1
004FEF99  6a 02                 push 2
004FEF9B  8d 45 d4              lea eax,[ebp-0x2c]
004FEF9E  50                    push eax
004FEF9F  8d 8d 48 fb ff ff     lea ecx,[ebp-0x4b8]
004FEFA5  e8 66 23 fb ff        call 0x004B1310
```

The call invokes the `CMapFile` load routine with the stack-local `CMapFile` in
`ecx`, a converted path argument on the stack, and two fixed integer arguments.
The path is valid for the duration of the call even though the `CMapFile` is
destroyed before the wrapper returns.

S4ModApi 2.3 supplies `hlib::CallPatch`, including strict expected-byte
construction, `patch()`, `update()`, `isPatched()`, and `unpatch()`. This is
preferred over a function-entry Detour because the call site is fixed-map-only,
requires no instruction relocation, and avoids random-map or preview noise.

Hooking the `CMapFile` function entry and returning to public-callback-only
inference are rejected alternatives. The first has broader traffic and
trampoline risk; the second already failed to establish loaded identity in
Phase 2.2.

## Executable and Hook-Site Admission

`HookSiteLayout` admits the site only when all checks pass:

- `S4_Main.exe` version equals `2.50.1516.0`;
- file SHA-256 equals
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- the live module size contains RVA `0x000FEFA5` plus five bytes;
- the complete five-byte span is inside the executable `.text` section;
- checked base-plus-RVA arithmetic does not overflow;
- live bytes equal `e8 66 23 fb ff` exactly;
- the decoded original relative target equals RVA `0x000B1310`;
- no existing patch or third-party modification is present at the site.

Any mismatch leaves public diagnostics and controlled stop available but keeps
the internal capture disabled. The implementation must not chain, overwrite,
or attempt to recognize an unknown third-party patch.

## Hook Adapter and Original Call

The x86 adapter matches the original MSVC `thiscall` boundary. It receives the
original `ecx` object, path argument, and integer arguments without changing
their meaning. A minimal ABI shim may use `__fastcall` with the conventional
dummy `edx` parameter when tests and generated disassembly prove the stack and
callee-cleanup behavior.

The adapter has two responsibilities:

1. attempt a bounded copy of the path into plugin-owned storage;
2. invoke the original load routine exactly once with the original arguments
   and return its result unchanged.

It does not keep the `CMapFile`, stack-string, or character-buffer pointer after
return. A null, malformed, inaccessible, or oversized path disables that
capture but must not prevent the original game call. No exception may cross
the adapter boundary.

The adapter may call only the original routine displaced by the selected
`CALL`. It may not invoke other internal game functions.

## Path Validation

The copied path is accepted only after all applicable checks pass:

- bounded MSVC x86 string invariants are valid for the observed storage type;
- the decoded string is non-empty, terminates within the fixed cap, and has no
  control character;
- separators normalize to backslashes;
- the path is not absolute, UNC, drive-qualified, or traversing with `..`;
- its first component is `Map`, case-insensitively;
- its extension is `.map`, case-insensitively;
- the final handle-resolved file is a regular file beneath the final
  handle-resolved game `Map` root;
- the emitted identity is a UTF-8, game-relative path.

Validation does not enumerate the Map tree or call `canonical` on an untrusted
path. Logs never contain a raw pointer, memory dump, or absolute user path.

## Capture and Session State

`FixedMapCaptureState` combines the existing calibrated `list_kind` with
validated Hook captures. Each accepted capture contains:

```text
FixedMapLoadCapture
- list_kind: single | multiplayer | custom | unknown
- map_relative_path
- capture_sequence
- captured_at_ms
- source: fixed-map-call-site
- failure_reason
```

The state machine follows these rules:

- a calibrated tab action opens a menu-selection epoch and latches its list
  kind through the subsequent selection/lobby transition;
- that list-kind lease expires after ten minutes and is invalidated immediately
  by another calibrated tab, Back, or return to a non-selection top-level menu;
- a capture is eligible only inside that unambiguous menu-selection epoch;
- identical repeated captures before `map-init` collapse into one candidate;
- two different valid paths before one `map-init` make the session ambiguous;
- a Hook capture is eligible for `map-init` for 15 seconds; elapsed time greater
  than or equal to 15,000 milliseconds is expired;
- `map-init` consumes the newest single unambiguous candidate exactly once;
- no candidate, an expired candidate, unknown `list_kind`, or ambiguity produces
  an explicit `unknown` loaded identity;
- returning to the menu starts a new menu-selection epoch and cannot reuse the
  previous loaded identity.

The Hook capture is the loaded identity source. Public UI attribution provides
the fixed-map list kind; it does not manufacture a map ID.

## Logging

Logging is change-based rather than frame-based. Records include only:

- Hook admission or one stable failure reason;
- Hook installation and verified restoration;
- list kind;
- normalized relative map path;
- capture sequence and menu epoch;
- `map-init` association result;
- ambiguity, expiry, path validation, or patch-conflict reason.

No record claims victory, completion, persistence, or marker status.

## Concurrency and Controlled Stop

The Hook path uses a callback gate separate from the public listener gate. Its
hot path must not perform filesystem I/O while executing inside the displaced
game call. It copies bounded data and publishes a small immutable pending
record; path/file validation occurs outside the adapter at a safe runtime
boundary.

Controlled stop executes in this order:

1. reject new Hook captures;
2. wait for all in-flight Hook adapters to return;
3. require the target bytes to still represent this plugin's installed patch;
4. call `hlib::CallPatch::unpatch()`;
5. reread the target and require exact restoration to `e8 66 23 fb ff`;
6. close and remove all 75 public S4ModApi listeners in reverse order;
7. release S4ModApi and leave the ASI inert.

If step 3 detects a third-party change, the plugin does not overwrite it. It
records a critical conflict, disables further internal activity, and makes live
acceptance `NO-GO`. The user must exit normally; the plugin never terminates the
game or Settlers United.

## Components

- `HookSiteLayout`: hash, PE-section, RVA, expected-byte, and original-target
  validation.
- `FixedMapLoadHook`: strict `CallPatch` ownership, install verification,
  callback gating, and restore verification.
- `FixedMapLoadAdapter`: the minimal x86 ABI boundary and original-call
  forwarding.
- `CapturedMapPath`: bounded plugin-owned path payload with no live game pointer.
- `MapPathValidator`: normalized relative-path and final-handle containment
  validation.
- `FixedMapCaptureState`: list attribution, epochs, sequences, ambiguity,
  expiry, and `map-init` consumption.
- `DiagnosticRuntime`: constructs the Hook only after executable compatibility
  and stops it before public listeners.

## TDD and CI Strategy

All behavior changes follow pushed RED/GREEN TDD. The local environment lacks
MSVC, so every C++ RED and GREEN is verified by the Win32 GitHub Actions runner.

Tests cover:

- correct executable identity, section, RVA, original bytes, and decoded target;
- wrong version/hash/section/bytes, integer overflow, and prepatched-site
  rejection;
- original function called exactly once with unchanged arguments and result;
- x86 stack balance and ABI behavior in the Win32 target;
- null, inaccessible, malformed, unterminated, oversized, escaping, missing,
  and non-map path rejection;
- valid Single, Multiplayer, and Custom capture association;
- identical-repeat collapse, different-path ambiguity, exact expiry boundary,
  one-time `map-init` consumption, and stale-epoch rejection;
- stop waiting for in-flight adapters;
- successful unpatch and exact byte restoration;
- refusal to overwrite a patch changed by a simulated third party.

CI continues to require archive integration tests, `/W4 /WX /permissive-`, the
complete CTest suite, PE machine `0x014c`, exact ASI/INI package contents, and
artifact upload.

## Deployment and Live Acceptance

Deployment uses only the existing guarded Settlers United archive workflow.
Before every replacement, the game and all Settlers United processes must be
closed, and the original backup hash must remain
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

The user launches normally through Settlers United and performs three separate
scenarios:

1. select and start one fixed map from Single Player Maps;
2. return normally, then select and start a different map from Multiplayer
   Maps;
3. return normally, then select and start a different map from Custom Maps.

Each scenario must produce one validated relative `.map` path, the calibrated
list kind, one `map-init` association, and no ambiguity. The three observations
must remain distinct when different maps were chosen.

The final scenario exercises controlled stop while the game remains responsive.
Acceptance requires exact restoration of the five original bytes, complete
zero-failure removal of 75 listeners, post-stop callback silence, continued
process responsiveness, all 12 protected hashes, installed archive integrity,
original backup integrity, embedded ASI identity, and restore readiness.

## Decision Rules

`GO` requires:

- all Hook-site admission checks pass;
- the original routine is invoked exactly once per intercepted call;
- Single, Multiplayer, and Custom each produce one stable loaded-map identity;
- every `map-init` consumes the intended capture without ambiguity;
- the Hook restores the exact original five bytes before listener shutdown;
- game stability, controlled stop, protected hashes, archive checks, and backup
  checks all pass.

`PARTIAL/NO-GO` applies when the Hook is safely installed and restored but one
or more list identities are absent, ambiguous, expired, or incorrectly
associated.

`NO-GO` applies to an ABI mismatch, skipped or duplicate original call, Hook
ownership conflict, failed byte restoration, process instability, safety-boundary
violation, protected-file mismatch, or backup/archive integrity failure.

No failure authorizes a second Hook, broader scan, guessed ABI, completion
feature, or additional game-file mutation.
