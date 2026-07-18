# Phase 3B Native Victory Event Subscription Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a calibration-only Win32 subscriber that transparently observes the game's native event `609` (`0x261`) and records whether the game reported the local alliance as won or lost.

**Architecture:** Admit three internal RVAs only for the exact approved executable, then use the game's own `RegisterHandle`/`UnRegisterHandle` operations without patching code or pointers. A process-lifetime subscriber copies four numeric event fields into a bounded per-map probe and always returns `false`; existing public S4ModApi callbacks perform same-thread attach/detach and drain records to a new allowlisted trace channel.

**Tech Stack:** C++17, Win32/x86 MSVC ABI, S4ModApi 2.3.2 public callbacks, native Settlers IV event registration, CMake 3.24+, MSVC `/W4 /WX /permissive-`, CTest, PowerShell policy/package verification, GitHub Actions `windows-2022`.

## Global Constraints

- Develop directly on `main`; do not create a worktree or feature branch.
- The game is currently running, so do not deploy or replace any game/SU file during implementation or CI work.
- Only the exact `S4_Main.exe` version `2.50.1516.0` and SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816` is admitted.
- Use native registration only: `InternalVictoryHook=0`, `HookCount=0`, `CodePatchBytes=0`.
- Never use `WriteProcessMemory`, `VirtualProtect`, an IAT/vtable/call/function patch, a Hook framework, a Lua write, a game-data write, or a victory-condition call.
- The native callback always returns `false` and performs no file I/O, allocation, Lua/API call, or retained game-pointer storage.
- Registration and unregistration occur only from a public S4ModApi UI-frame or non-delayed Tick callback on the game thread.
- Keep output calibration-only: `CompletionDetection=0`, `CompletionStorage=0`, `CompletionMarkers=0`, and exactly `diagnostic-result=calibration-only`.
- Keep `CaptureTraceRoot=` empty in the packaged INI; live output remains below `F:\claude projects\thesettler4plugin\artifacts\phase-3-victory-diagnostics`.
- Authoritative compilation and tests run in Win32 GitHub Actions; require two clean runs of the same final commit before any deployment request.

---

## File Structure

- `src/victory/VictoryEventProbe.h/.cpp`: pure bounded per-map event state, deduplication, conflict handling, and pending snapshots.
- `src/native/NativeEventAdmission.h/.cpp`: exact executable/RVA/section/opcode/pointer-slot admission; no mutation.
- `src/native/NativeEventRegistration.h/.cpp`: narrow wrappers around native `RegisterHandle` and `UnRegisterHandle`.
- `src/native/NativeVictoryEventSubscriber.h/.cpp`: ABI-compatible transparent handler and attach/detach state machine.
- `tests/VictoryEventProbeTests.cpp`: pure event-result behavior.
- `tests/NativeEventAdmissionTests.cpp`: fail-closed layout behavior.
- `tests/NativeVictoryEventSubscriberTests.cpp`: transparent callback and lifecycle behavior with a fake registrar.
- `src/diagnostics/Phase3Trace.h/.cpp` and `tests/Phase3TraceTests.cpp`: fifth, separately allowlisted `native-event.trace` channel.
- `src/runtime/DiagnosticRuntime.h/.cpp`, `src/runtime/S4Listeners.h/.cpp`, and `src/dllmain.cpp`: ownership, game-thread service, trace draining, and detach-before-stop.
- `config/CampaignCompletionDebug.ini`, `tests/RuntimePolicyTests.cpp`, `tools/verify_no_process_patch.ps1`, `.github/workflows/build-debug-asi.yml`: version/policy enforcement and mutation fixtures.
- `docs/research/phase-3b-native-victory-event-report.md`: CI, artifact, deployment, and live-control evidence.

### Task 1: Add the bounded per-session event probe and trace channel

**Files:**
- Create: `src/victory/VictoryEventProbe.h`
- Create: `src/victory/VictoryEventProbe.cpp`
- Create: `tests/VictoryEventProbeTests.cpp`
- Modify: `src/diagnostics/Phase3Trace.h`
- Modify: `src/diagnostics/Phase3Trace.cpp`
- Modify: `tests/Phase3TraceTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: copied `NativeEventFields {eventId, wParam, lParam, gameTick}` only.
- Produces: `VictoryEventProbe::BeginSession`, `Observe`, `ConsumePending`, `Disable`; `VictoryEventSnapshot`; `Phase3TraceChannel::NativeEvent`.

- [ ] **Step 1: Write failing probe and trace tests**

Add tests that require this public interface before it exists:

```cpp
VictoryEventProbe probe;
probe.BeginSession(7u);
Require(probe.Observe({609u, 1u, 0u, 123u}), "609/1 is observed");
const auto won = probe.ConsumePending();
Require(won.has_value() && won->sessionId == 7u &&
            won->result == NativeLocalResult::Won && won->gameTick == 123u,
        "609/1 is a local win");

Require(!probe.Observe({600u, 1u, 0u, 124u}),
        "unrelated events are ignored");
Require(probe.Observe({609u, 0u, 0u, 125u}),
        "opposite terminal result creates a conflict");
Require(probe.ConsumePending()->result == NativeLocalResult::Conflict,
        "conflict fails closed");
```

Cover `609/0`, invalid `wParam`, identical duplicates with a saturating count,
orphan events, a new session reset, zero session, disabled state, and one-shot
`ConsumePending`. Extend the trace test to require a fifth file and accept only:

```text
native-subscription=attached
native-event=session-7;event-id=609;local-result=won;wparam=1;lparam=0;game-tick=123
native-event-duplicates=session-7;count=0
native-event-orphans=count=1
```

Require the new channel still rejects `0x261`, `victory-confirmed`, absolute
paths, newlines, and records over 1,024 bytes.

- [ ] **Step 2: Push the tests and verify RED in Win32 CI**

Run locally:

```bash
git diff --check
git add tests/VictoryEventProbeTests.cpp tests/Phase3TraceTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify native victory event evidence"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: the Win32 build fails because `victory/VictoryEventProbe.h` and
`Phase3TraceChannel::NativeEvent` do not exist. Record the run and exact failure.

- [ ] **Step 3: Implement the minimal probe and fifth trace channel**

Define the exact types:

```cpp
inline constexpr std::uint32_t kNativeTerminalEventId = 609u;

enum class NativeLocalResult { None, Won, Lost, Malformed, Conflict };

struct NativeEventFields final {
    std::uint32_t eventId = 0u;
    std::uint32_t wParam = 0u;
    std::uint32_t lParam = 0u;
    std::uint32_t gameTick = 0u;
};

struct VictoryEventSnapshot final {
    std::uint64_t sessionId = 0u;
    NativeEventFields fields{};
    NativeLocalResult result = NativeLocalResult::None;
    std::uint32_t duplicates = 0u;
    std::uint32_t orphans = 0u;
};

class VictoryEventProbe final {
public:
    void BeginSession(std::uint64_t sessionId) noexcept;
    bool Observe(NativeEventFields fields) noexcept;
    std::optional<VictoryEventSnapshot> ConsumePending() noexcept;
    void Disable() noexcept;
};
```

Protect state with one mutex. Ignore non-609 events. Treat invalid `wParam` as
`Malformed`; retain the first terminal state, saturate duplicate/orphan counts,
and convert any different later terminal result to `Conflict`. `Disable` clears
pending state and rejects future observations.

Expand the trace enum/handle arrays/file names from four to five. Add a
`native-event.trace` filename and a channel-specific prefix allowlist while
leaving the existing global forbidden tokens unchanged.

- [ ] **Step 4: Run tests and commit GREEN**

Run the authoritative workflow after pushing:

```bash
git add src/victory/VictoryEventProbe.h src/victory/VictoryEventProbe.cpp \
  src/diagnostics/Phase3Trace.h src/diagnostics/Phase3Trace.cpp \
  tests/VictoryEventProbeTests.cpp tests/Phase3TraceTests.cpp \
  tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: record bounded native terminal events"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: Win32 build and CTest pass this task; existing trace rejection tests
remain green.

### Task 2: Admit the exact native event interface without patching

**Files:**
- Create: `src/native/NativeEventAdmission.h`
- Create: `src/native/NativeEventAdmission.cpp`
- Create: `tests/NativeEventAdmissionTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: approved `ModuleInfo` and read-only PE/live-memory evidence.
- Produces: `NativeEventAdmission::Create`, `FromEvidenceForTesting`, and an admitted engine-slot/register/unregister address triple.

- [ ] **Step 1: Write failing admission tests**

Use this exact specification:

```cpp
inline constexpr NativeEventLayoutSpec kApprovedNativeEventLayout{
    0x0106B11Cu,  // event-engine pointer slot
    0x0005A8A0u,  // RegisterHandle
    0x0005A990u,  // UnRegisterHandle
    {0x55u, 0x8Bu, 0xECu, 0x6Au, 0xFFu},
    {0x55u, 0x8Bu, 0xECu, 0x51u, 0xA1u},
};
```

Tests must individually reject version mismatch, hash mismatch, module/RVA
overflow, non-executable operation sections, unreadable slot, unreadable engine
object, both opcode mismatches, and a live global-operand mismatch. Require one
fully matching evidence set to produce all three nonzero addresses.

- [ ] **Step 2: Push and verify RED**

```bash
git add tests/NativeEventAdmissionTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: require fail-closed native event admission"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: Win32 compilation fails because `native/NativeEventAdmission.h` is
missing.

- [ ] **Step 3: Implement deterministic admission**

Define:

```cpp
enum class NativeEventAdmissionFailure {
    None, VersionMismatch, HashMismatch, FileUnavailable, InvalidPeImage,
    TextSectionMissing, OutsideModule, WrongSection, AddressOverflow,
    MemoryUnavailable, RegisterBytesMismatch, UnregisterBytesMismatch,
    EngineSlotOperandMismatch, EngineUnavailable,
};

struct NativeEventAdmission final {
    std::uintptr_t engineSlot = 0u;
    std::uintptr_t registerHandle = 0u;
    std::uintptr_t unregisterHandle = 0u;
    NativeEventAdmissionFailure failure = NativeEventAdmissionFailure::None;
    explicit operator bool() const noexcept;
};
```

Read the PE headers and executable section with the same bounded Win32 pattern
used by `HookSiteLayout`, but do not include or link any Hook-era source. Read
the approved file bytes, verify the exact module identity first, verify live
committed memory with `VirtualQuery`, and use SEH-protected `memcpy` for bounded
live reads. Check that relocated operands in the register/unregister instruction
windows resolve to `moduleBase + 0x0106B11C`. Never scan memory or write it.

- [ ] **Step 4: Run tests and commit GREEN**

```bash
git add src/native/NativeEventAdmission.h src/native/NativeEventAdmission.cpp \
  tests/NativeEventAdmissionTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: admit exact native event interface"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: admission tests and the full suite pass.

### Task 3: Implement transparent registration and lifecycle

**Files:**
- Create: `src/native/NativeEventRegistration.h`
- Create: `src/native/NativeEventRegistration.cpp`
- Create: `src/native/NativeVictoryEventSubscriber.h`
- Create: `src/native/NativeVictoryEventSubscriber.cpp`
- Create: `tests/NativeVictoryEventSubscriberTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: admitted addresses and `VictoryEventProbe`.
- Produces: `INativeEventRegistration`, production `NativeEventRegistration`, and `NativeVictoryEventSubscriber` with game-thread `Service`, `RequestDetach`, and transparent `OnEvent`.

- [ ] **Step 1: Write failing lifecycle tests with a fake registrar**

Require these behaviors:

```cpp
FakeRegistration registrar;
VictoryEventProbe probe;
NativeVictoryEventSubscriber subscriber;
Require(subscriber.Prepare(registrar, probe), "subscriber prepares once");
Require(subscriber.state() == NativeSubscriptionState::AttachRequested,
        "prepare requests attach");
subscriber.ServiceOnGameThread();
Require(registrar.registerCalls == 1u &&
            subscriber.state() == NativeSubscriptionState::Attached,
        "game-thread service attaches exactly once");

NativeEventObjectView event{};
event.eventId = 609u;
event.wParam = 1u;
Require(!subscriber.OnEvent(event), "native callback never consumes event");

subscriber.RequestDetach();
subscriber.ServiceOnGameThread();
Require(registrar.unregisterCalls == 1u && subscriber.Detached(),
        "game-thread service detaches exactly once");
```

Also test attach failure, detach failure, duplicate service, malformed/null
event safety through the exported adapter, disabled probe, and the invariant
that every callback path returns `false`.

- [ ] **Step 2: Push and verify RED**

```bash
git add tests/NativeVictoryEventSubscriberTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify transparent native event subscriber"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: Win32 compilation fails because the subscriber interface is absent.

- [ ] **Step 3: Implement the minimal subscriber and production registrar**

Use the x86 event layout proved from the exact executable:

```cpp
struct NativeEventObjectView final {
    std::uintptr_t vtable = 0u;
    std::uint32_t eventId = 0u;
    std::uint32_t wParam = 0u;
    std::uint32_t lParam = 0u;
    std::uint32_t gameTick = 0u;
};
static_assert(offsetof(NativeEventObjectView, eventId) == 0x04u);

class NativeVictoryEventSubscriber final {
public:
    virtual bool OnEvent(const NativeEventObjectView& event) noexcept;
    bool Prepare(INativeEventRegistration& registration,
                 VictoryEventProbe& probe) noexcept;
    void ServiceOnGameThread() noexcept;
    void RequestDetach() noexcept;
    NativeSubscriptionState state() const noexcept;
    bool Detached() const noexcept;
};
```

Do not add a virtual destructor: the first and only virtual slot must be the
native `OnEvent` ABI. Read event fields inside an MSVC SEH boundary, copy them
to `VictoryEventProbe`, catch all C++ exceptions, and return `false` regardless
of outcome.

The production registrar uses `bool (__thiscall*)(void*, void*)` functions at
the admitted operation addresses, reads the current engine pointer through the
admitted slot under SEH, never caches the engine/event pointer, and forwards the
native Boolean result.

- [ ] **Step 4: Run tests and commit GREEN**

```bash
git add src/native/NativeEventRegistration.h src/native/NativeEventRegistration.cpp \
  src/native/NativeVictoryEventSubscriber.h src/native/NativeVictoryEventSubscriber.cpp \
  tests/NativeVictoryEventSubscriberTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: subscribe transparently to native terminal events"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: subscriber tests and the full suite pass with `/W4 /WX`.

### Task 4: Integrate game-thread service, tracing, and detach-before-stop

**Files:**
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `src/dllmain.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`

**Interfaces:**
- Consumes: admission, registrar, subscriber, probe, current map-init session.
- Produces: same-thread attach/detach, `native-event.trace` records, and safe controlled-stop ordering.

- [ ] **Step 1: Write failing runtime ordering tests**

Require source-level and pure-state evidence that:

- runtime version/mode becomes `0.3.1` / `native-event-calibration`;
- runtime owns admission, registration, process-lifetime subscriber, and probe;
- `S4Listeners::Start` receives subscriber and probe;
- both UI-frame and non-delayed Tick call `ServiceNativeSubscription()` before
  taking the listener mutex;
- map init calls `victoryProbe_->BeginSession(activeSessionId_)`;
- event trace formatting uses decimal `event-id=609` and neutral `won/lost`;
- controlled stop requests native detach, waits while public listeners remain
  registered, verifies `Detached()`, then disables components, removes public
  listeners, and closes the trace;
- a detach timeout emits `controlled-stop-flush=failed` and does not destroy or
  unload the still-registered subscriber;
- the exported stop function requests controlled stop rather than directly
  mutating the native event list.

- [ ] **Step 2: Push and verify RED**

```bash
git add tests/RuntimePolicyTests.cpp
git commit -m "test: require detach-before-stop event integration"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: CTest fails on the first missing `0.3.1`/native-subscriber ordering
requirement while compilation and unrelated tests pass.

- [ ] **Step 3: Implement runtime integration**

At startup, create admission from the already enumerated exact executable and
prepare the registrar/subscriber, but do not register on the bootstrap thread.
Pass subscriber/probe to `S4Listeners`. At the beginning of UI-frame and
non-delayed Tick callbacks, call `ServiceOnGameThread`; drain state changes and
probe snapshots afterward.

Format bounded records through helpers:

```cpp
native-subscription=attached
native-event=session-1;event-id=609;local-result=won;wparam=1;lparam=0;game-tick=12345
native-event-duplicates=session-1;count=0
```

`DiagnosticRuntime::RequestControlledStop()` sets an atomic request. The
control loop requests subscriber detachment and waits in 10 ms increments for
at most 5,000 ms while public callbacks remain active. On success it performs
the existing disable/remove/flush sequence. On timeout it records failure and
keeps the runtime and process-lifetime subscriber alive; it retries on later
control-loop iterations rather than closing resources underneath registration.

- [ ] **Step 4: Run tests and commit GREEN**

```bash
git add src/runtime/S4Listeners.h src/runtime/S4Listeners.cpp \
  src/runtime/DiagnosticRuntime.h src/runtime/DiagnosticRuntime.cpp \
  src/dllmain.cpp tests/RuntimePolicyTests.cpp
git commit -m "feat: integrate native terminal event calibration"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: full Win32 build and CTest pass; no completion decision is emitted.

### Task 5: Enforce configuration and zero-patch policy

**Files:**
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `tools/verify_no_process_patch.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Consumes: linked Phase 3B source closure and built PE32 ASI.
- Produces: machine-checked one-subscription/zero-patch policy and adversarial mutation rejection.

- [ ] **Step 1: Write failing policy expectations**

Require this exact configuration:

```ini
Version=0.3.1
DiagnosticMode=NativeVictoryEventCalibration
NativeEventSubscription=1
NativeTerminalEventId=609
InternalVictoryHook=0
HookCount=0
CodePatchBytes=0
GameDefaultGameEndCheckCalls=0
LuaWrites=0
GameDataWrites=0
CompletionDetection=0
CompletionStorage=0
CompletionMarkers=0
```

Update the verifier contract to allow only the named native registration
sources while continuing to reject every patch framework/API and forbidden
victory-condition token. Add mutation fixtures for `WriteProcessMemory`,
`VirtualProtect`, `hlib::CallPatch`, `hlib::JmpPatch`, `MinHook`,
`GameDefaultGameEndCheck`, `VICTORY_CONDITION_CHECK`, and Lua writes.

- [ ] **Step 2: Run policy verifier and confirm RED**

Push the policy test only and run CI. Expected: CTest fails because the INI is
still version `0.3.0` and declares no native subscription.

- [ ] **Step 3: Update configuration, verifier, and workflow**

Keep `PublicSettlementUiProbe=1` for cross-checking, but change the verifier
success message to describe “native event calibration, zero process patches,
and read-only Lua bridge.” Require the ASI target to link the four new
production sources and no `src/hook/*` source.

- [ ] **Step 4: Run full CI and commit GREEN**

```bash
git add config/CampaignCompletionDebug.ini tests/RuntimePolicyTests.cpp \
  tools/verify_no_process_patch.ps1 .github/workflows/build-debug-asi.yml
git commit -m "test: enforce zero-patch native event policy"
git push origin main
gh workflow run build-debug-asi.yml --ref main
```

Expected: archive integration, Win32 configure/build, policy verifier, every
mutation fixture, CTest, package layout, PE32 check, and artifact upload pass.

### Task 6: Final verification, evidence freeze, and deployment gate

**Files:**
- Create: `docs/research/phase-3b-native-victory-event-report.md`
- Create only below ignored project artifacts: `artifacts/phase-3b-ci/...`
- Modify after later live work: `docs/research/phase-3b-native-victory-event-report.md`

**Interfaces:**
- Consumes: final commit and two clean Win32 artifacts.
- Produces: immutable hashes and a deployment/live-calibration checklist; no deployment while the game remains open.

- [ ] **Step 1: Run the complete verification set twice from one commit**

Run two separate `build-debug-asi` workflow dispatches without changing the
commit. For both, require all steps listed in Task 5 to pass. Record run IDs,
job IDs, commit SHA, artifact IDs, and GitHub artifact digests.

- [ ] **Step 2: Freeze and verify the later artifact**

Download the later `CampaignCompletionDebug-Win32` artifact below
`artifacts/phase-3b-ci/<run-id>-attempt-<n>/`. Record SHA-256 and size for the
download, inner ZIP, ASI, and INI. Require exactly:

```text
Plugins/CampaignCompletionDebug.asi
CampaignCompletion/CampaignCompletionDebug.ini
```

Verify PE machine `0x014c`, version `0.3.1`, one empty `CaptureTraceRoot=`, one
native subscription, zero Hooks/patch bytes/writes/completion behavior, and no
unexpected archive entry.

- [ ] **Step 3: Write and commit the pre-deployment report**

Document source evidence, the no-code-patch architecture, every RED/GREEN CI
run, final hashes, and the outstanding live controls. Explicitly state that no
game/SU file was modified because the game remained running.

```bash
git add docs/research/phase-3b-native-victory-event-report.md
git commit -m "docs: freeze native event calibration artifact"
git push origin main
```

- [ ] **Step 4: Stop at the deployment gate**

Do not deploy automatically. Ask the user to close the game. Only after the
user confirms closure may the existing guarded backup/hash/archive workflow
replace the authorized project-owned ASI/INI and guarded `Plugin_SU.zip`.

- [ ] **Step 5: Collect live controls one at a time after a later approved deployment**

Collect voluntary-exit (`no 609`), normal victory (`609/1`), normal defeat
(`609/0`), and load-before-victory (`609/1` with recovered origin/identity)
samples. Verify responsiveness after each. Finish with controlled-stop evidence
showing native detach before public listener removal and successful trace flush.
Do not enable completion storage or markers in this plan.
