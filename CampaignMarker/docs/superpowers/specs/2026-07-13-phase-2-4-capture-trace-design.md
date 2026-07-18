# Phase 2.4 Bounded Capture Trace Design

Date: 2026-07-13  
Target: The Settlers IV History Edition through Settlers United  
Branch: `main`

## Context and Goal

Phase 2.3 installed the approved single `hlib::CallPatch` at RVA
`0x000FEFA5`. Live Single Player Maps testing established a calibrated
`single` menu epoch and reached `map-init`, but produced
`identity unknown reason=no-candidate`. The current runtime cannot distinguish
between an adapter that was never entered and an adapter whose bounded
MSVC-x86 wide-string capture failed.

Phase 2.4 adds only enough evidence to distinguish those boundaries. It is a
diagnostic phase, not a map-identity fix. It must not select a new Hook site,
add another Hook, change completion behavior, or claim that a map identity is
confirmed without the existing validation chain.

## Mutation and Authorization Boundary

The internal patch remains the same one five-byte process-local `CALL`
replacement at RVA `0x000FEFA5`. No other process memory is modified. The
original call is still forwarded exactly once with the same arguments and ABI.

Disk writes are limited to:

- the already authorized project-owned `CampaignCompletion*.asi` deployment;
- the authorized `CampaignCompletion` configuration directory;
- guarded replacement of Settlers United `Plugin_SU.zip` after verifying the
  immutable original backup;
- a new diagnostic trace beneath the explicitly configured project trace root.

The trace never writes to a map, save, campaign, Settlers United resource, or
unrelated game file. The plugin never terminates the game or Settlers United.

## Alternatives Considered

### Selected: bounded in-process event trace

The existing adapter publishes a small owned diagnostic event. A safe runtime
boundary writes a compact trace at `map-init` or controlled stop. This directly
distinguishes no invocation, wide-string capture failure, path-validation
failure, and successful association without pausing the game.

### Rejected for this phase: external debugger breakpoint

A debugger could prove whether the address executes, but it pauses the game and
can perturb Settlers United timing. It also gives weaker repeatability than a
bounded event recorded by the deployed artifact.

### Rejected: immediately replace the Hook site

The current evidence does not yet prove that the site is unexecuted. Selecting
another site before observing the failed boundary would be speculative and
would require a separate static review and authorization decision.

## Event Data Flow

The Hook hot path performs no file-system access and writes no log. For every
adapter entry it creates one bounded owned event containing:

- monotonically increasing capture sequence;
- capture timestamp;
- `WideCaptureFailure` result;
- the owned wide string only when capture succeeds.

The event is passed to `FixedMapIdentityProbe` even when wide capture fails.
The probe retains at most eight events, matching the existing capture bound.
It stores no raw pointer, memory dump, absolute map path, or game object.

At `map-init`, the probe evaluates the event chain and sends one compact record
to the dedicated trace sink:

- no event: `map-init-association=no-candidate adapter-entered=0`;
- event with failed wide capture:
  `map-init-association=wide-<reason> adapter-entered=<count>`;
- decoded value rejected by path validation:
  `map-init-association=path-<reason> adapter-entered=<count>`;
- validated identity:
  `map-init-association=confirmed adapter-entered=<count>` plus list kind,
  normalized game-relative path, sequence, and menu epoch.

Repeated identical failure states are emitted once per menu epoch. Overflow is
reported explicitly and never promoted to a confirmed identity.

## Dedicated Project Trace

The trace root is supplied by the diagnostic INI as `CaptureTraceRoot`. The
packaged default is empty, which disables this extra trace. During authorized
live deployment it is set to the Windows path corresponding to:

`dist/phase-2-4/runtime/`

The implementation reads only this single diagnostic setting. It resolves the
configured directory through a directory handle, rejects a non-directory or
reparse escape, and writes only below the final resolved configured root. It
does not create missing parent directories; deployment prepares the directory
inside the project first.

Each process creates a new file named `capture-trace-<pid>.log`. Creation uses
exclusive create-new semantics. If the name exists, the sink tries bounded
numeric suffixes and never truncates or appends to an existing file. Failure to
validate or create the trace disables the dedicated sink but does not block the
original game call, public listeners, controlled stop, or existing general
diagnostics.

The dedicated file contains only:

- `adapter-entered` counts at a safe flush boundary;
- `wide-capture=<success|reason>`;
- `path-validation=<success|reason>`;
- `map-init-association=<confirmed|reason>`;
- controlled-stop flush status.

It excludes module inventory, UI pages, mouse events, GUI elements, pointers,
memory contents, absolute map paths, victory, completion, persistence, and
marker claims. Existing `CampaignCompletion.log` keeps its current behavior;
general records are not copied into the dedicated trace.

## Error Handling and Concurrency

The Hook callback gate and strict original-call forwarding remain unchanged.
Event publication is bounded and exception-contained. Trace I/O occurs only
outside the displaced game call, at `map-init` or controlled stop.

Trace failures are fail-open for gameplay and fail-closed for evidence: the
game call continues, but no inference is made from a missing or incomplete
trace. Controlled stop still rejects new captures, drains the Hook gate,
verifies patch ownership, restores the exact original bytes, then removes all
75 public listeners in reverse order.

## TDD and CI

RED tests must first demonstrate that the current implementation cannot
distinguish these two cases:

1. no adapter event before `map-init`;
2. an adapter event whose `CapturedWidePath` contains a specific failure.

Additional RED tests specify successful capture, path rejection, event
overflow, trace-root rejection, exclusive file creation, non-overwrite suffix
selection, and the dedicated log allowlist. GREEN adds only the minimum event
forwarding and trace sink required by those tests.

CI must retain all existing gates and additionally prove:

- the Win32 adapter still cleans exactly 12 stack bytes (`ret 0x0C`);
- production contains exactly one `hlib::CallPatch` construction and no jump,
  NOP, Detour, or MinHook backend;
- Hook hot-path source performs no logging or file-system operation;
- the complete Win32 tests pass under `/W4 /WX /permissive-`;
- the ASI remains PE machine `0x014c`;
- the package entry policy remains unchanged;
- the dedicated trace contains only allowlisted record types.

## Deployment and Live Decision

Deployment uses the existing guarded archive workflow. Before replacement it
must verify the original backup SHA-256
`807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.
The frozen ASI, installed archive entry, and deployed INI are hashed before the
game starts.

Live acceptance uses one user-driven Single Player Maps / Aeneas launch after
an explicit click on calibrated tab ID `2449`:

- `adapter-entered=0`: the approved call site was not traversed; Phase 2.4 is
  evidence-complete and the site remains NO-GO for identity capture;
- `adapter-entered>0` with `wide-capture=<reason>`: investigate only that
  decoding boundary;
- wide capture succeeds but path validation fails: investigate only the
  reported path boundary;
- association is confirmed: resume the separate Single, Multiplayer, and
  Custom live acceptance sequence.

After the live run, controlled stop must report successful Hook restoration,
75 registered and 75 removed listeners with zero failures, and continued game
responsiveness. Any patch conflict, crash, hang, missing trace, or ambiguous
event is NO-GO.
