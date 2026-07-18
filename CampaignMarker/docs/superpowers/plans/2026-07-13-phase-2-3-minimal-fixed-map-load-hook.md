# Phase 2.3 Minimal Fixed-Map Load Hook Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture and validate one loaded fixed-map path from each calibrated fixed-map list by replacing one hash-gated five-byte game `CALL`, while preserving the original call exactly once and restoring the original bytes before shutdown.

**Architecture:** A static evidence gate fixes the x86 ABI and one numeric call-site layout. A strict `hlib::CallPatch` backend redirects that call to a minimal `__fastcall` adapter, which copies an x86 MSVC wide string into plugin-owned memory and forwards the original call exactly once. A separate state machine combines the capture with public list attribution and `map-init`; controlled stop drains the adapter, restores the original bytes, then removes public listeners.

**Tech Stack:** C++17, MSVC Win32/x86, S4ModApi 2.3.2 `hlib::CallPatch`, Win32 `VirtualQuery`/SEH and handle-based path validation, CMake/CTest, GNU `objdump`, PowerShell 5+, GitHub Actions.

## Global Constraints

- Target only `S4_Main.exe` file version `2.50.1516.0` with SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- The only internal patch site is RVA `0x000FEFA5`; expected live bytes are exactly `e8 66 23 fb ff`, decoding to original target RVA `0x000B1310`.
- The adapter ABI is x86 `__fastcall` with a dummy `edx` parameter and three stack parameters; its generated return must pop exactly `0x0C` bytes.
- Use only S4ModApi `hlib::CallPatch` for the process-local five-byte replacement. Do not add a function-entry Detour, custom trampoline, second Hook, heap scan, or unrelated internal call.
- A hash, PE section, live byte, decoded target, patch ownership, path, timing, or identity mismatch fails closed to `unknown` or disables the Hook. It never authorizes a broader fallback.
- The Hook hot path copies at most 512 UTF-16 code units into plugin-owned memory, performs no filesystem I/O, and retains no game pointer.
- A list-kind lease lasts 600,000 ms and is invalidated by another calibrated tab, Back ID `2415`, or a top-level page `1` or `7` observation outside the fixed-map set.
- A Hook capture is eligible for `map-init` only while elapsed time is `< 15,000 ms`; `>= 15,000 ms` is expired.
- The Hook path is stopped before the existing 75 public S4ModApi listeners. Stop must drain in-flight adapters and restore `e8 66 23 fb ff` before listener removal.
- Do not add campaigns, random maps, saved games, multiplayer session classification, victory detection, completion storage, completion markers, or user-facing rendering.
- Game and Settlers United files remain read-only except project-owned `CampaignCompletion*.asi`, the `CampaignCompletion` configuration directory, and the already authorized guarded replacement of `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
- Preserve `research/backups/settlers-united/Plugin_SU.zip.original` exactly; never commit proprietary archives, ASIs, runtime logs, raw memory, or backup metadata.
- Never deploy while `S4_Main.exe` or Settlers United is running, and never terminate either process automatically.
- Develop directly on `main`; do not create a worktree or feature branch.
- Follow pushed RED/GREEN TDD for every C++ behavior change.

## CI TDD Protocol

The workspace has no local MSVC toolchain. Every C++ RED/GREEN step uses the
Windows workflow. After each push:

```bash
run_id="$("/mnt/c/Program Files/GitHub CLI/gh.exe" run list \
  --workflow build-debug-asi.yml --branch main --limit 1 \
  --json databaseId --jq '.[0].databaseId')"
"/mnt/c/Program Files/GitHub CLI/gh.exe" run watch "$run_id" --exit-status
```

For RED, the archive test must pass and the named missing header/symbol or new
assertion must fail. For GREEN, archive integration, Win32 configure/build,
complete CTest, package, PE32 check, and artifact upload must all pass. Record
every run ID in the final report.

---

## Planned File Structure

- `research/scripts/collect_fixed_map_hook_evidence.sh`: hash-gated bounded disassembly and ABI extraction.
- `research/evidence/fixed-map-hook-site.txt`: normalized call-site, target, string-layout, and export evidence.
- `docs/research/phase-2-3-fixed-map-hook-evidence.md`: reviewed numeric layout and accept/reject gate.
- `src/hook/HookSiteLayout.h/.cpp`: executable, section, bytes, and relative-target admission.
- `src/hook/MsvcX86WideString.h/.cpp`: bounded SEH-protected copy from the observed x86 string layout.
- `src/identity/MapPathValidator.h/.cpp`: safe game-relative `.map` validation.
- `src/identity/FixedMapCaptureState.h/.cpp`: leases, epochs, ambiguity, expiry, and `map-init` consumption.
- `src/identity/FixedMapIdentityProbe.h/.cpp`: UI/capture orchestration, validation, and change-based diagnostic records.
- `src/hook/CallPatchBackend.h`: testable patch ownership interface.
- `src/hook/HlibCallPatchBackend.h/.cpp`: production S4ModApi `hlib::CallPatch` wrapper.
- `src/hook/FixedMapLoadHook.h/.cpp`: adapter gate, original invoker, installation, and restoration.
- `src/runtime/S4Listeners.h/.cpp`: forwards calibrated list actions, invalidation events, and `map-init`.
- `src/runtime/DiagnosticRuntime.h/.cpp`: construction and Hook-before-listener stop ordering.
- `tests/HookSiteLayoutTests.cpp`, `tests/MsvcX86WideStringTests.cpp`, `tests/MapPathValidatorTests.cpp`, `tests/FixedMapCaptureStateTests.cpp`, `tests/FixedMapIdentityProbeTests.cpp`, and `tests/FixedMapLoadHookTests.cpp`: focused unit/Win32 tests.
- `tools/verify_hook_adapter.ps1`: PE disassembly verification for the adapter's `ret 0x0c` and absence of an extra patch site.
- `config/CampaignCompletionDebug.ini`: explicit single-Hook diagnostic policy and version `0.2.3`.
- `docs/research/phase-2-3-fixed-map-load-hook-report.md`: CI, artifact, deployment, three live identities, restoration, stop, and integrity decision.

## Task 1: Static Hook and ABI Evidence Gate

**Files:**
- Create: `research/scripts/collect_fixed_map_hook_evidence.sh`
- Create: `research/evidence/fixed-map-hook-site.txt`
- Create: `docs/research/phase-2-3-fixed-map-hook-evidence.md`

**Interfaces:**
- Consumes: approved `S4_Main.exe`, `S4_MainR.pdb`, PE section headers, Phase 2.2 evidence, `third_party/S4ModApi/S4ModApi.h`, and `S4ModApi.lib` exports.
- Produces: one accepted numeric `FixedMapHookSiteSpec` row containing call-site RVA, original target RVA, exact five bytes, argument count/order, callee cleanup, path layout, and evidence instructions.
- Gate: if any ABI or target fact is ambiguous, stop Phase 2.3 as `NO-GO`; do not compile or install a Hook.

- [ ] **Step 1: Add the deterministic collector**

The script sources `research/scripts/common.sh`, verifies the EXE/PDB against
`research/evidence/manifest.sha256`, and refuses an EXE hash other than the
approved value. It emits only bounded data:

```bash
readonly CALL_SITE_VMA=0x004fefa5
readonly CALL_SITE_RVA=0x000fefa5
readonly ORIGINAL_TARGET_VMA=0x004b1310
readonly ORIGINAL_TARGET_RVA=0x000b1310
readonly EXPECTED_CALL_BYTES="e8 66 23 fb ff"
readonly S4MODAPI_DLL="$GAME_DIR/S4ModApi.dll"

emit_disassembly 'fixed-map caller ABI' 0x004fef8f 0x004fefb0
emit_disassembly 'CMapFile load ABI' 0x004b1310 0x004b1460
emit_disassembly 'x86 MSVC wide-string length capacity and storage' 0x00458150 0x00458250
emit_disassembly 'converted path construction' 0x004fef21 0x004fefa6
objdump -h "$EXE"
objdump -p "$S4MODAPI_DLL" | sed -n '/Export Tables/,/Import Tables/p'
```

It must calculate the signed `rel32` target from the captured bytes and assert:

```text
0x004FEFAA + sign_extend32(0xFFFB2366) = 0x004B1310
```

- [ ] **Step 2: Prove deterministic collection**

Run twice and require an empty normalized diff:

```bash
bash research/scripts/collect_fixed_map_hook_evidence.sh
cp research/evidence/fixed-map-hook-site.txt /tmp/fixed-map-hook-site.first
bash research/scripts/collect_fixed_map_hook_evidence.sh
diff -u \
  <(sed '/^generated_utc=/d;/^workspace_commit=/d' /tmp/fixed-map-hook-site.first) \
  <(sed '/^generated_utc=/d;/^workspace_commit=/d' research/evidence/fixed-map-hook-site.txt)
```

- [ ] **Step 3: Review and document the ABI**

The report table contains exact instruction VMAs/RVAs and bytes for:

```text
caller pushes: 1, 2, &wstring; ECX=&CMapFile; CALL at 0x004FEFA5
callee reads: [EBP+0x08], [EBP+0x0C], [EBP+0x10]
callee cleanup: C2 0C 00 at 0x004B141F
wstring reads: length +0x10, capacity +0x14, pointer +0x00 when capacity >= 8
```

The accepted initializer must be fully numeric:

```cpp
inline constexpr FixedMapHookSiteSpec kApprovedFixedMapHookSite{
    0x000FEFA5u,
    0x000B1310u,
    {0xE8u, 0x66u, 0x23u, 0xFBu, 0xFFu},
    0x0000000Cu,
};
```

- [ ] **Step 4: Commit and push the evidence gate**

```bash
git diff --check
git add research/scripts/collect_fixed_map_hook_evidence.sh \
  research/evidence/fixed-map-hook-site.txt \
  docs/research/phase-2-3-fixed-map-hook-evidence.md
git commit -m "research: establish fixed-map hook ABI"
git push origin main
```

Expected: no binary or raw memory is committed; the decision must be
`ACCEPTED FOR SINGLE CALL-SITE HOOK` before Task 2.

## Task 2: Hook-Site Layout Admission

**Files:**
- Create: `src/hook/HookSiteLayout.h`
- Create: `src/hook/HookSiteLayout.cpp`
- Create: `tests/HookSiteLayoutTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `FixedMapHookSiteSpec`, `HookSiteFailure`, `HookSiteAdmission`, `HookSiteLayout::Create(const ModuleInfo&)`, and `HookSiteLayout::FromEvidenceForTesting(...)`.
- `HookSiteAdmission` exposes only resolved call-site/original-target addresses and a stable failure; it does not patch or dereference the target function.

- [ ] **Step 1: Push RED layout tests**

Define tests with a synthetic module base `0x00400000`, size `0x012BF000`, and
`.text` containing both RVAs:

```cpp
const auto ok = HookSiteLayout::FromEvidenceForTesting(
    ApprovedModule(), TextSection(), kApprovedFixedMapHookSite,
    {0xE8, 0x66, 0x23, 0xFB, 0xFF});
Require(ok.failure == HookSiteFailure::None, "approved site admitted");
Require(ok.callSite == 0x004FEFA5u, "call site resolved");
Require(ok.originalTarget == 0x004B1310u, "target decoded");

Require(AdmitWithHash("wrong").failure == HookSiteFailure::HashMismatch,
        "wrong hash rejected");
Require(AdmitWithBytes({0x90, 0x90, 0x90, 0x90, 0x90}).failure ==
            HookSiteFailure::BytesMismatch,
        "prepatched site rejected");
Require(AdmitWithSection(ReadOnlyData()).failure ==
            HookSiteFailure::WrongSection,
        "site outside text rejected");
Require(AdmitWithTargetBytes({0xE8, 0, 0, 0, 0}).failure ==
            HookSiteFailure::TargetMismatch,
        "wrong rel32 target rejected");
```

Push only tests/registration and require CI failure at missing
`hook/HookSiteLayout.h`.

- [ ] **Step 2: Implement file-backed PE and live-byte admission**

`Create` first calls `CheckTargetExecutable`, parses the installed executable
file read-only to admit `.text`, resolves checked base-plus-RVA addresses, and
reads exactly five live bytes after `VirtualQuery` confirms committed executable
memory. Copy those bytes inside MSVC `__try/__except`. Decode `rel32` using
signed 64-bit arithmetic and require the approved target.

The public result shape is:

```cpp
struct HookSiteAdmission final {
    std::uintptr_t callSite = 0;
    std::uintptr_t originalTarget = 0;
    HookSiteFailure failure = HookSiteFailure::None;
    explicit operator bool() const noexcept {
        return failure == HookSiteFailure::None;
    }
};
```

- [ ] **Step 3: Push GREEN and require complete CI**

```bash
git add CMakeLists.txt src/hook/HookSiteLayout.* \
  tests/HookSiteLayoutTests.cpp tests/TestMain.cpp
git commit -m "feat: admit fixed-map hook site"
git push origin main
```

Require the CI TDD protocol to finish GREEN.

## Task 3: Bounded x86 MSVC Wide-String Capture

**Files:**
- Create: `src/hook/MsvcX86WideString.h`
- Create: `src/hook/MsvcX86WideString.cpp`
- Create: `tests/MsvcX86WideStringTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CapturedWidePath`, `WideCaptureFailure`, and `CaptureMsvcX86WideString(const void* object, std::size_t maxChars) noexcept`.
- The function accepts only the reviewed 24-byte x86 layout and returns an owned `std::wstring`; it exposes no source pointer.

- [ ] **Step 1: Push RED capture tests**

Use a synthetic exact layout:

```cpp
#pragma pack(push, 1)
struct TestWideString32 final {
    union { wchar_t inlineText[8]; std::uint32_t pointer32; } storage;
    std::uint32_t length;
    std::uint32_t capacity;
};
#pragma pack(pop)
static_assert(sizeof(TestWideString32) == 24);
```

Require SSO capture, heap capture from a low 32-bit `VirtualAlloc` address,
length greater than capacity, capacity/length over 512, null, no-access source,
no-access heap target, missing terminator, and address overflow rejection. The
successful fixtures are `L"Map\\User\\Stable.map"` and an SSO value.

Push RED and require missing `hook/MsvcX86WideString.h`.

- [ ] **Step 2: Implement the minimal SEH-protected copy**

Read the 24-byte header only after `VirtualQuery`. Require:

```text
length <= capacity
length < maxChars
capacity < 0x00100000
capacity < 8 => characters are inline
capacity >= 8 => storage.pointer32 is nonzero readable memory
characters[length] == L'\0'
```

Copy exactly `length + 1` UTF-16 units inside `__try/__except`, then return an
owned string excluding the terminator. Do not allocate based on unvalidated
capacity.

- [ ] **Step 3: Push GREEN and run CI twice**

```bash
git add CMakeLists.txt src/hook/MsvcX86WideString.* \
  tests/MsvcX86WideStringTests.cpp tests/TestMain.cpp
git commit -m "feat: capture bounded x86 map paths"
git push origin main
```

Require GREEN, rerun the same successful workflow once, and require the rerun
GREEN so page cleanup and SEH fixtures execute on two independent workers.

## Task 4: Fixed-Map Path Validation

**Files:**
- Create: `src/identity/MapPathValidator.h`
- Create: `src/identity/MapPathValidator.cpp`
- Create: `tests/MapPathValidatorTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MapPathResult MapPathValidator::ValidateWide(std::wstring_view raw) const` and pure `IsFinalPathBeneath(root, candidate)`.
- Success returns one UTF-8, backslash-separated path relative to the game directory.

- [ ] **Step 1: Push RED filesystem tests**

In a temporary game root create `Map\User\Stable.map`. Require acceptance of
`Map/User/Stable.map` and case-insensitive lookup. Require explicit failure for
empty/control input, absolute/UNC/drive paths, `.`/`..`, non-Map root, wrong
extension, directory, missing file, and a reparse/final-path escape. Require
component-boundary containment so `MapSibling` is not beneath `Map`.

Push RED and require missing `identity/MapPathValidator.h`.

- [ ] **Step 2: Implement exact validation order**

Normalize separators, reject roots and traversal before filesystem use, open
the Map root and candidate read/share-only, reject directories, resolve both
handles with `GetFinalPathNameByHandleW`, and require case-insensitive
component-boundary containment. Convert the accepted game-relative path with
`WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)`.

- [ ] **Step 3: Push GREEN**

```bash
git add CMakeLists.txt src/identity/MapPathValidator.* \
  tests/MapPathValidatorTests.cpp tests/TestMain.cpp
git commit -m "feat: validate captured fixed-map paths"
git push origin main
```

Require complete GREEN CI.

## Task 5: Capture Epoch and `map-init` State

**Files:**
- Create: `src/identity/FixedMapCaptureState.h`
- Create: `src/identity/FixedMapCaptureState.cpp`
- Create: `tests/FixedMapCaptureStateTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `BeginMenuEpoch(FixedMapListKind, std::uint64_t nowMs)`, `InvalidateMenuEpoch()`, `ObserveCapture(std::string relativePath, std::uint64_t sequence, std::uint64_t nowMs)`, and `ObserveMapInit(std::uint64_t nowMs)`.
- Produces immutable `LoadedMapIdentity` with list kind, relative path, sequence, menu epoch, confidence, and `CaptureFailure`.

- [ ] **Step 1: Push RED state-machine tests**

Use constants `kListLeaseMs = 600000` and `kCaptureLeaseMs = 15000`. Require:

```cpp
state.BeginMenuEpoch(FixedMapListKind::Single, 1000);
state.ObserveCapture("Map\\User\\A.map", 1, 2000);
Require(state.ObserveMapInit(16999).confidence == Confirmed,
        "capture valid below expiry boundary");

state.BeginMenuEpoch(FixedMapListKind::Single, 1000);
state.ObserveCapture("Map\\User\\A.map", 2, 2000);
Require(state.ObserveMapInit(17000).failure == CaptureFailure::Expired,
        "capture expired at exact boundary");
```

Also require identical repeat collapse; different-path ambiguity; unknown list;
expired list lease at 600,000 ms; another tab creates a new epoch; Back/top-level
invalidation; `map-init` consumes once; a previous epoch cannot be reused; and
three distinct Single/Multiplayer/Custom observations remain distinct.

Push RED and require missing `identity/FixedMapCaptureState.h`.

- [ ] **Step 2: Implement deterministic transitions**

Store at most eight pending captures. Overflow produces `CaptureFailure::Overflow`
and no identity. Compare paths case-insensitively. Identical repeats preserve the
lowest sequence and newest timestamp; different paths set an irreversible
ambiguity flag for the current epoch. `ObserveMapInit` always clears pending
captures after returning a result.

- [ ] **Step 3: Push GREEN**

```bash
git add CMakeLists.txt src/identity/FixedMapCaptureState.* \
  tests/FixedMapCaptureStateTests.cpp tests/TestMain.cpp
git commit -m "feat: associate fixed-map captures with sessions"
git push origin main
```

Require complete GREEN CI.

## Task 6: Single Call-Patch Ownership and Adapter

**Files:**
- Create: `src/hook/CallPatchBackend.h`
- Create: `src/hook/HlibCallPatchBackend.h`
- Create: `src/hook/HlibCallPatchBackend.cpp`
- Create: `src/hook/FixedMapLoadHook.h`
- Create: `src/hook/FixedMapLoadHook.cpp`
- Create: `tests/FixedMapLoadHookTests.cpp`
- Create: `tools/verify_hook_adapter.ps1`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ICallPatchBackend`: `Install(callSite, destination, expectedBytes)`, `Inspect()`, and `Restore()` with stable `PatchState`/`PatchFailure`.
- `IOriginalLoadInvoker::Invoke(void* mapFile, const void* path, DWORD mode, DWORD validate)` is the sole original-call seam.
- `IRawCaptureSink::Observe(CapturedWidePath, std::uint64_t sequence, std::uint64_t nowMs)` receives owned captures.
- `FixedMapLoadHook::Start(HookSiteAdmission, ICallPatchBackend&, IOriginalLoadInvoker&, IRawCaptureSink&)` and `Stop()` own the Hook lifecycle.

- [ ] **Step 1: Push RED ownership and forwarding tests**

Use fake backend/invoker/sink classes. Require:

```cpp
Require(!hook.Start(FailedAdmission(), backend, invoker, sink),
        "failed layout cannot patch");
Require(hook.Start(ApprovedAdmission(), backend, invoker, sink),
        "approved layout patches once");
CampaignCompletionFixedMapAdapter(fakeMapFile, nullptr, &wideFixture, 2, 1);
Require(invoker.calls == 1, "original invoked exactly once");
Require(invoker.lastMode == 2 && invoker.lastValidate == 1,
        "original arguments preserved");
Require(sink.records.size() == 1, "owned capture published");
```

Also require malformed capture still calls the original once; backend install
failure; post-install inspect failure; second start rejection; stop waits for an
in-flight adapter; restore occurs after drain; restored-state verification;
third-party state prevents restore overwrite and returns conflict; and adapter
after gate closure cannot publish a new capture.

Push RED and require missing `hook/FixedMapLoadHook.h`.

- [ ] **Step 2: Implement the generic lifecycle and adapter**

Declare the exported bridge ABI exactly:

```cpp
extern "C" __declspec(dllexport) void __fastcall
CampaignCompletionFixedMapAdapter(void* mapFile, void* ignoredEdx,
                                  const void* pathObject, DWORD mode,
                                  DWORD validate) noexcept;
```

`Start` sets the original invoker and sink before backend installation, installs
once, and requires `Inspect() == PatchState::InstalledByUs`. The exported bridge
dispatches to the active `FixedMapLoadHook`, obtains a callback lease, attempts
`CaptureMsvcX86WideString(pathObject, 512)`, publishes only owned data when the
gate accepts it, then invokes the original exactly once. Capture failures are
isolated; the original call is unconditional.

`Stop` closes and drains the adapter gate, requires ownership, restores, and
requires `PatchState::Original` before clearing the active dispatcher.

- [ ] **Step 3: Implement only the production hlib backend**

Keep `HlibCallPatchBackend.h` independent of S4ModApi through a private
implementation pointer. Compile `HlibCallPatchBackend.cpp` into the ASI target
only, not the unit-test target, so tests do not require S4ModApi.dll. The literal
type name `hlib::CallPatch` appears only in that `.cpp` construction.

Construct a strict patch with no NOPs:

```cpp
static constexpr hlib::Patch::BYTE5 kExpected{{0xE8, 0x66, 0x23, 0xFB, 0xFF}};
patch_ = std::make_unique<hlib::CallPatch>(
    static_cast<UINT64>(callSite), static_cast<DWORD>(destination),
    &kExpected, 0);
```

Before `patch()` and before `unpatch()`, independently read the five bytes and
classify them as original, installed-by-us, or conflict. Never call `unpatch()`
after conflict classification.

The production original invoker resolves `0x000B1310` from the admitted layout
and calls:

```cpp
using OriginalLoad = void(__thiscall*)(void*, const void*, DWORD, DWORD);
reinterpret_cast<OriginalLoad>(originalTarget)(mapFile, path, mode, validate);
```

- [ ] **Step 4: Verify generated x86 ABI**

`tools/verify_hook_adapter.ps1` runs `dumpbin /EXPORTS` and `/DISASM` on the built
ASI, locates the exported `CampaignCompletionFixedMapAdapter` bridge, and
requires one `ret 0Ch`. It fails if the adapter returns with plain `ret`,
`ret 10h`, or if the source/binary contains another `CallPatch`/`JmpPatch`
construction site.
Add this script after the Build step and before CTest in the workflow.

- [ ] **Step 5: Push GREEN and rerun CI once**

```bash
git add CMakeLists.txt .github/workflows/build-debug-asi.yml \
  src/hook/CallPatchBackend.h src/hook/HlibCallPatchBackend.* \
  src/hook/FixedMapLoadHook.* tests/FixedMapLoadHookTests.cpp \
  tests/TestMain.cpp tools/verify_hook_adapter.ps1
git commit -m "feat: add single fixed-map call hook"
git push origin main
```

Require GREEN, including ABI script, then rerun and require GREEN again.

## Task 7: Probe and Public-Callback Integration

**Files:**
- Create: `src/identity/FixedMapIdentityProbe.h`
- Create: `src/identity/FixedMapIdentityProbe.cpp`
- Create: `tests/FixedMapIdentityProbeTests.cpp`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `FixedMapIdentityProbe` implements `IRawCaptureSink` and exposes `ObserveListKind`, `ObserveBack`, `ObserveTopLevelPage`, `ObserveMapInit`, and `Disable`.
- `S4Listeners::Start` receives `FixedMapIdentityProbe&` and forwards only calibrated events.
- `DiagnosticRuntime` owns probe, path validator, Hook backend, and Hook; it stops the Hook before listeners.

- [ ] **Step 1: Push RED orchestration tests**

Inject a temporary Map root and synthetic owned captures. Require:

- Single tab + valid capture + `map-init` emits one confirmed Single identity;
- Multiplayer and Custom produce their own kinds and distinct paths;
- Back ID `2415`, page `1`, and page `7` invalidate the epoch;
- lobby/mission pages `26`, `30`, and `37` do not invalidate it;
- invalid/missing path, ambiguity, expiry, and capture overflow emit one stable
  `unknown` reason;
- repeated frames and repeated identical callbacks emit no duplicate record;
- `Disable` permanently rejects new captures;
- formatted output contains no raw pointer, absolute game directory, `victory`,
  `completion`, or `marker`.

Push RED and require missing `identity/FixedMapIdentityProbe.h`.

- [ ] **Step 2: Implement probe and listener forwarding**

`S4Listeners` keeps its existing `ListAttribution`. On a recognized tab release,
it calls `ObserveListKind(kind, now)`. On Back ID `2415` inside the exact fixed-map
set, it calls `ObserveBack`. When a stable snapshot has only top-level page `1`
or `7`, it calls `ObserveTopLevelPage`. `OnMapInit` calls probe association before
writing the legacy `map-init observed` line.

The probe validates owned raw strings only outside the Hook adapter, passes valid
paths to `FixedMapCaptureState`, and logs only state changes.

- [ ] **Step 3: Integrate startup and strict stop order**

After exact executable compatibility and S4ModApi availability:

1. create `HookSiteLayout`;
2. construct validator/probe;
3. construct original invoker and hlib backend;
4. start the Hook;
5. start public listeners.

If Hook admission/start fails, retain public diagnostics and controlled stop,
but log one disabled reason. `Stop` calls `fixedMapHook_.Stop()` before
`listeners_.Stop()`. A Hook conflict makes the phase `NO-GO` but never triggers
blind restoration. If public listener startup fails after a successful Hook
installation, startup must immediately call `fixedMapHook_.Stop()` and require
successful byte restoration before releasing S4ModApi.

Set the INI policy exactly:

```ini
[Diagnostic]
Version=0.2.3
LogLevel=Info
ModuleInventory=1
PublicListeners=1
FixedMapLoadHook=1
HookSiteRva=0x000FEFA5
HookCount=1
CodePatchBytes=5
GameDataWrites=0
UnrelatedInternalCalls=0
CompletionDetection=0
CompletionStorage=0
CompletionMarkers=0
ControlledStopFile=CampaignCompletionDebug.stop
```

Update runtime header to `version=0.2.3 hook-mode=single-call-site`.

- [ ] **Step 4: Push GREEN and run safety/package checks**

Run:

```bash
rg -n "MinHook|MH_CreateHook|CreateRemoteThread|JmpPatch|NopPatch" src
rg -n "hlib::CallPatch" src
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File tools/test_settlers_united_artifact.ps1
```

Expected: first scan has no match; the `CallPatch` scan identifies only the
single backend construction; archive integration passes.

```bash
git add CMakeLists.txt config/CampaignCompletionDebug.ini \
  src/identity/FixedMapIdentityProbe.* src/runtime/DiagnosticRuntime.* \
  src/runtime/S4Listeners.* tests/FixedMapIdentityProbeTests.cpp \
  tests/RuntimePolicyTests.cpp tests/TestMain.cpp
git commit -m "feat: integrate fixed-map identity hook"
git push origin main
```

Require complete GREEN CI and a package containing exactly the ASI and INI.

## Task 8: Live Three-List Acceptance and Report

**Files:**
- Create: `docs/research/phase-2-3-fixed-map-load-hook-report.md`
- Modify only if evidence correction is required: `docs/research/phase-2-3-fixed-map-hook-evidence.md`

**Interfaces:**
- Consumes: exact successful CI artifact, guarded SU archive tools, user-driven three-list launches, plugin log, live call-site bytes, protected hashes, and backup metadata.
- Produces: `GO`, `PARTIAL/NO-GO`, or `NO-GO` with reproducible evidence.

- [ ] **Step 1: Freeze the exact artifact**

Record commit, workflow/run/job/artifact IDs, artifact digest, downloaded package
SHA-256, ASI/INI SHA-256, PE machine, exact ZIP entries, and adapter ABI-script
result. Require artifact commit equals `origin/main` and all checks GREEN.

Count installed recursive regular `*.map` files and require baseline `373`.

- [ ] **Step 2: Guarded deployment**

Ask the user to close the game and Settlers United. Confirm no relevant process.
Require:

```text
original backup SHA-256 = 807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265
installed archive SHA-256 = current recorded patchedSha256
restore readiness = true
```

The user runs the existing elevated installer. Reopen the archive read-only and
require the embedded ASI hash equals the frozen CI ASI.

- [ ] **Step 3: Read the original call-site bytes before launch actions**

After normal launch and before selecting a map, use only plugin diagnostics to
record:

```text
hook-site admitted rva=0x000FEFA5 original-target-rva=0x000B1310
hook installed count=1 ownership=verified
```

Do not use an external writer or debugger to alter process memory.

- [ ] **Step 4: Run three separate fixed-map launches**

With one action at a time and log review between actions:

1. Single Player Maps: click the calibrated tab, select map A, launch, remain
   after `map-init`, then return normally without winning.
2. Multiplayer Maps: click the calibrated tab, select a different map B, use the
   game's normal local-host flow without joining an external session, remain
   after `map-init`, then return normally. If the installed game cannot start
   that flow safely, record the case as untested and cap the decision at
   `PARTIAL/NO-GO`.
3. Custom Maps: click the calibrated tab, select a different map C, launch,
   remain after `map-init`, then return normally.

For each require exactly one confirmed record containing expected `list_kind`,
validated game-relative `.map` path, capture sequence/menu epoch, and one
`map-init` association. Require A, B, and C are distinct when the selected files
are distinct. Any unknown/ambiguous/expired result remains explicit.

- [ ] **Step 5: Prove Hook restoration and controlled stop**

While the game is responsive, create only the authorized stop file. Require log
order:

```text
controlled stop requested
fixed-map hook gate closed in-flight=0
fixed-map hook restored bytes=e8,66,23,fb,ff
listeners stopped registered=75 removed=75 failures=0
diagnostic runtime stopped
```

After user-driven menu navigation, require no later Hook/UI/mouse/map-init lines
and continued process responsiveness. A restore conflict or missing exact-byte
proof is `NO-GO`.

- [ ] **Step 6: Recompute integrity and publish the report**

Require `sha256sum -c research/evidence/manifest.sha256` reports 12/12 `OK`.
Record installed archive, original backup, embedded ASI, complete ZIP tests, and
restore readiness. Run the archive integration test and source scans.

The report includes static ABI rows, every RED/GREEN run, artifact hashes, three
identity records, every failure, patch ownership/restoration, stop silence,
protected hashes, and one decision using the design rules.

```bash
git diff --check
git add docs/research/phase-2-3-fixed-map-load-hook-report.md \
  docs/research/phase-2-3-fixed-map-hook-evidence.md
git diff --cached --name-only
git commit -m "docs: report phase 2.3 fixed-map hook results"
git push origin main
```

Expected staged paths: the report and only an actually corrected evidence file;
never an ASI, archive, log, dump, or backup artifact.

## Final Verification Checklist

- [ ] Static evidence proves the five bytes, rel32 target, three stack arguments, `ret 0x0c`, and x86 wide-string layout.
- [ ] Exact EXE identity, `.text` ownership, live original bytes, and decoded target are required before patching.
- [ ] Source contains exactly one `hlib::CallPatch` construction and no other patch mechanism.
- [ ] Adapter ABI verification finds `ret 0x0c`; original call forwarding tests prove exactly once.
- [ ] The Hook hot path retains no game pointer and performs no filesystem I/O.
- [ ] Single, Multiplayer, and Custom identities are confirmed or the decision is not `GO`.
- [ ] Controlled stop drains the Hook, restores exact original bytes, then removes all 75 listeners.
- [ ] Post-stop silence and process responsiveness pass.
- [ ] All 12 protected hashes, archive, embedded ASI, immutable backup, and restore readiness pass.
- [ ] No victory, completion persistence, completion marker, campaign, random-map, or saved-game behavior exists.
- [ ] `git status --short` is empty and `HEAD == origin/main`.
