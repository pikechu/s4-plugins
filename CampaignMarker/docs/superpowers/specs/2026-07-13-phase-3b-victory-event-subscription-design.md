# Phase 3B Native Victory Event Subscription Design

## Status

Approved direction: observe the game's native `0x261` terminal-result event
instead of classifying the after-game summary UI. This phase remains
calibration-only: it records bounded evidence and does not persist completion
or render a completed-map marker.

## Evidence and Decision

Settlers IV compares the winning team with the local player's alliance before
constructing event `0x261`. It supplies `wParam=1` for a local-alliance victory
and `wParam=0` for a local defeat. The same local result is reconstructed when
an already-won game state is loaded. A voluntary exit without a winning team
does not use either victory-event construction path.

S4ModApi 2.3.2 does not publish an event-engine listener. Read-only inspection
of the approved executable nevertheless confirms that the native event engine
has its own `RegisterHandle` and `UnRegisterHandle` operations and dispatches
each event to registered handlers. A handler that returns `false` observes the
event without consuming it.

Phase 3B therefore uses native event registration, not a code-patching Hook:

- `HookCount=0`;
- `CodePatchBytes=0`;
- no import-address, vtable, call-site, or function-entry replacement;
- no Lua calls or writes;
- no game-data writes;
- no invocation of a victory-condition check.

## Scope

### Included

- Admit the native event-engine interface only for the already-approved
  `S4_Main.exe` version `2.50.1516.0` and SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Register one transparent native event handler.
- Observe only event `0x261` and copy only its numeric `wParam`, `lParam`, and
  game tick.
- Associate one terminal result with the current map-init session.
- Record `won`, `lost`, malformed, duplicate, and conflict
  evidence below the existing project-scoped Phase 3 trace root.
- Keep the existing launch/load origin and SU Lua map-identity observations.
- Keep the after-game UI probe temporarily as a terminal-window cross-check;
  it is not an outcome classifier.
- Prove transparent registration, bounded observation, fail-closed admission,
  and same-thread detach in automated tests and live controls.

### Excluded

- Completion JSON or any other completion persistence.
- Completed-level marker rendering.
- Treating an event by itself as sufficient for eventual completion; source
  eligibility and confirmed map identity remain mandatory downstream gates.
- Reading GUI text or classifying player rows/icons.
- Calling `Game.DefaultGameEndCheck`, wrapping Lua victory handlers, or reading
  internal winner/team state.
- Any modification of game-directory files during implementation or testing.

## Native ABI Admission

Admission is tied to the exact executable identity above. The admitted layout
contains three RVAs recovered from the user's exact executable:

- event-engine pointer slot: `0x0106B11C`;
- `RegisterHandle`: `0x0005A8A0`;
- `UnRegisterHandle`: `0x0005A990`.

Before registration, the implementation must prove all of the following:

1. the executable identity is exact;
2. both operation RVAs are inside executable PE sections;
3. their bounded opcode fingerprints match the approved executable;
4. the pointer slot is inside the mapped image and readable;
5. the event-engine pointer is non-null and points to readable committed
   memory;
6. registration reports success.

Any failed or ambiguous check disables the native subscriber and records a
single fail-closed diagnostic. No fallback address guessing or broad memory
scan is permitted.

## Event ABI

The native handler object contains a single MSVC x86 virtual entry matching
`bool __thiscall OnEvent(CEvn_Event&)`. The observed layout is:

- offset `+0x04`: event ID;
- offset `+0x08`: `wParam`;
- offset `+0x0C`: `lParam`;
- offset `+0x10`: game tick.

The callback first validates that the event pointer and bounded numeric fields
are readable. It copies those four numeric values and never retains the game
pointer. It always returns `false`, including on malformed input, exceptions,
disabled state, duplicate events, and trace failures. Consequently it cannot
consume or replace the game's event.

## Threading and Lifetime

Native event-list mutation is not assumed to be thread-safe. Admission may be
prepared on the bootstrap thread, but registration and unregistration occur
only from an existing public S4ModApi UI-frame or non-delayed Tick callback on
the game thread.

The subscriber has an explicit state machine:

1. `unprepared`;
2. `attach-requested`;
3. `attached`;
4. `detach-requested`;
5. `detached`;
6. `faulted`.

Controlled stop requests detachment first while public callbacks remain
active. A game-thread callback performs `UnRegisterHandle`; only after a
verified successful detach may the runtime remove public listeners and close
the trace. A bounded timeout is a stop failure, not permission to destroy a
still-registered handler. The subscriber object has process lifetime so a
timeout cannot leave a dangling native vtable pointer.

## Per-Session Observation

`VictoryEventProbe` owns copied event state independently of the native ABI.
Each successful map init starts a new nonzero session and clears the previous
terminal result.

For event `0x261`:

- `wParam=1` becomes `won`;
- `wParam=0` becomes `lost`;
- any other value becomes `malformed` and can never prove victory.

The first valid result is retained. An identical repeat increments a bounded
duplicate counter. A later different result marks the session `conflict` and
can never prove victory. Events received without an active session are
recorded only as bounded orphan diagnostics.

All other event IDs return immediately without allocation, logging, or state
change.

## Trace Contract

The callback itself performs no file I/O. Existing public callbacks drain
copied snapshots into the project-scoped trace. Records are numeric and
bounded, for example:

```text
native-subscription=attached
native-event=session-1;event-id=609;local-result=won;wparam=1;lparam=0;game-tick=12345
native-event-duplicates=session-1;count=0
diagnostic-result=calibration-only
```

These records use the decimal event ID and the neutral result words `won` and
`lost`. The existing global trace rejection of `0x`, `victory`, completion,
and marker claims remains intact.

No trace is written outside the configured project root. The packaged INI
keeps `CaptureTraceRoot=` empty.

## Runtime Integration

`DiagnosticRuntime` owns the admission metadata and process-lifetime native
subscriber. `S4Listeners` services attach/detach requests on the game thread,
starts `VictoryEventProbe` sessions at map init, and drains copied results on
non-delayed Tick/UI frames.

The current settlement probe remains installed only for calibration timing
and negative-control correlation. Its zero-feature result cannot suppress or
create a native victory result. Phase 3B continues to emit exactly
`diagnostic-result=calibration-only` and does not create completion state.

## Verification

Automated tests must prove:

- exact executable/layout admission and every fail-closed branch;
- `0x261/1`, `0x261/0`, malformed, duplicate, conflict, orphan, reset, and
  disabled behavior;
- unrelated events are ignored;
- the native handler always returns `false`;
- registration/unregistration occur only through the game-thread service
  method;
- failed registration never produces observations;
- failed or timed-out detach prevents a successful controlled-stop claim;
- no code-patch backend is linked into the ASI;
- configuration and policy checks state one internal subscription, zero
  Hooks, zero patched bytes, and zero completion behavior;
- Win32 x86 `/W4 /WX /permissive-`, CTest, package layout, and PE32 checks all
  pass twice from the same commit.

Live calibration requires, one at a time:

1. voluntary exit: no `0x261` result for that session;
2. normal eligible-map victory: exactly one `0x261/1` result;
3. normal eligible-map defeat: exactly one `0x261/0` result;
4. load-before-victory: recovered source/identity plus exactly one later
   `0x261/1` result;
5. responsiveness after every sample;
6. controlled stop with verified native detach before public listener removal.

Only after these controls pass may a later phase combine native victory with
source eligibility and confirmed map identity to persist completion.

## File and Deployment Safety

Development changes only project files. While the game is running, no ASI,
INI, Settlers United archive, or other game/SU file is replaced. A later
deployment requires the game to be closed and retains the already-approved
guarded backup/hash/archive procedure. Only project-owned
`CampaignCompletion*.asi`, the project-owned `CampaignCompletion` directory,
and the separately authorized guarded `Plugin_SU.zip` replacement are in
deployment scope.
