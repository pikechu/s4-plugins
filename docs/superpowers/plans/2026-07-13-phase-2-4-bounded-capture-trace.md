# Phase 2.4 Bounded Capture Trace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Win32 diagnostic ASI that distinguishes an untraversed fixed-map call site from bounded wide-string and path-validation failures, writing only allowlisted evidence to a unique project trace.

**Architecture:** Keep the approved single `hlib::CallPatch` and x86 ABI unchanged. Forward every bounded capture result, including failures, to `FixedMapIdentityProbe`; classify at `map-init` and send compact records to a new `CaptureTrace`. `DiagnosticRuntime` alone reads the optional INI trace root and owns file lifecycle outside the Hook hot path.

**Tech Stack:** C++17, Win32 API, S4ModApi 2.3.2, CMake 3.24+, MSVC Win32 `/W4 /WX /permissive-`, PowerShell, GitHub Actions `windows-2022`.

## Global Constraints

- Work directly on `main`; no worktree or feature branch.
- The only process patch remains the five-byte `CALL` at RVA `0x000FEFA5`, expected bytes `e8 66 23 fb ff`, original target RVA `0x000B1310`.
- Construct exactly one `hlib::CallPatch`; add no jump/NOP patch, MinHook, Detours, heap scan, unrelated internal call, or game-data write.
- Retain adapter `__fastcall`, generated `ret 0x0C`, and exactly-once original forwarding with unchanged arguments.
- Hook hot path performs no file/log I/O and retains at most eight owned events.
- Packaged `CaptureTraceRoot` is empty. Only the deployed authorized INI uses `F:\claude projects\thesettler4plugin\dist\phase-2-4\runtime`.
- Trace creation is exclusive and never overwrites/appends to an existing file.
- Dedicated records are limited to `adapter-entered`, `wide-capture`, `path-validation`, `map-init-association`, and `controlled-stop-flush`.
- Never put pointers, memory content, absolute map paths, module/UI/mouse data, victory, completion, persistence, or marker claims in the dedicated trace.
- Modify no game/SU file except authorized `CampaignCompletion*.asi`, the `CampaignCompletion` config directory, and guarded `Plugin_SU.zip` replacement.
- Preserve original archive SHA-256 `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.
- Never terminate the game or SU. The user performs launches, navigation, exits, and responsiveness checks.
- Local MSVC is unavailable: push RED and GREEN commits to `main` and verify each transition in GitHub Actions.

## File Structure

- `src/hook/FixedMapLoadHook.cpp`: publish successful and failed bounded results.
- `src/identity/FixedMapIdentityProbe.h/.cpp`: classify capture boundaries and emit safe trace records.
- `src/diagnostics/CaptureTrace.h/.cpp`: validate root, create a unique file, enforce the allowlist.
- `src/runtime/DiagnosticRuntime.h/.cpp`: read INI, own trace, wire and close it.
- `tests/FixedMapLoadHookTests.cpp`, `tests/FixedMapIdentityProbeTests.cpp`, `tests/CaptureTraceTests.cpp`: RED/GREEN behavior.
- `tests/RuntimePolicyTests.cpp`, `tools/verify_hook_adapter.ps1`: static scope gates.
- `config/CampaignCompletionDebug.ini`: version 0.2.4 and empty packaged trace root.
- `docs/research/phase-2-4-bounded-capture-trace-report.md`: frozen/live evidence.

---

### Task 1: Preserve Failed Adapter Captures

**Files:**
- Modify: `tests/FixedMapLoadHookTests.cpp`
- Modify: `src/hook/FixedMapLoadHook.cpp`

**Interfaces:**
- Consumes: `CaptureMsvcX86WideString` and `IRawCaptureSink::Observe`.
- Produces: one sink observation for every gated adapter entry, successful or failed.

- [ ] **Step 1: Write the RED assertion**

Replace the malformed-capture expectation in the approved fixture with:

```cpp
CampaignCompletionFixedMapAdapter(fakeMapFile, nullptr, nullptr, 3u, 0u);
Require(invoker.calls == 2,
        "malformed capture still invokes original exactly once");
Require(sink.Size() == 2u,
        "malformed capture publishes a bounded failure event");
Require(sink.records.back().failure ==
            campaign_completion::WideCaptureFailure::NullObject,
        "malformed capture preserves the exact failure reason");
Require(sink.sequences.back() == sink.sequences.front() + 1u,
        "every adapter entry receives a monotonic sequence");
```

Update the post-stop size assertion from one to two.

- [ ] **Step 2: Commit/push RED and verify**

```bash
git add tests/FixedMapLoadHookTests.cpp
git commit -m "test: require failed adapter capture events"
git push origin main
```

Expected: CTest fails at `malformed capture publishes...`; ABI verification passes.

- [ ] **Step 3: Implement minimal GREEN**

In `FixedMapLoadHook::Dispatch`, retain ordering and change only publication:

```cpp
auto capture = CaptureMsvcX86WideString(pathObject, 512u);
if (sink_ != nullptr) {
    try {
        sink_->Observe(std::move(capture), sequence, GetTickCount64());
    } catch (...) {
    }
}
```

`InvokeOriginal(...)` remains after observation and executes once.

- [ ] **Step 4: Commit/push GREEN and rerun**

```bash
git add src/hook/FixedMapLoadHook.cpp
git commit -m "feat: preserve failed adapter captures"
git push origin main
```

Require the complete workflow to pass twice, including `ret 0x0C`, one `CallPatch`, CTest, packaging, and PE `0x014c`.

---

### Task 2: Classify Capture Boundaries

**Files:**
- Modify: `tests/FixedMapIdentityProbeTests.cpp`
- Modify: `src/identity/FixedMapIdentityProbe.h`
- Modify: `src/identity/FixedMapIdentityProbe.cpp`

**Interfaces:**
- Consumes: all `CapturedWidePath` results from Task 1.
- Produces: optional `TraceSink = std::function<void(std::string)>` and safe-boundary records.

- [ ] **Step 1: Add RED no-entry and failure cases**

Construct separate sinks:

```cpp
std::vector<std::string> records;
std::vector<std::string> trace;
FixedMapIdentityProbe probe(
    gameRoot,
    [&](std::string value) { records.push_back(std::move(value)); },
    [&](std::string value) { trace.push_back(std::move(value)); });
```

Add:

```cpp
probe.ObserveListKind(FixedMapListKind::Single, 100u);
probe.ObserveMapInit(200u);
Require(Contains(trace, "adapter-entered=0"), "no entry is explicit");
Require(Contains(trace, "map-init-association=no-candidate"),
        "no entry stays no-candidate");

probe.ObserveListKind(FixedMapListKind::Single, 300u);
probe.Observe({{}, campaign_completion::WideCaptureFailure::NullObject},
              1u, 400u);
probe.ObserveMapInit(500u);
Require(Contains(trace, "adapter-entered=1"), "failure proves entry");
Require(Contains(trace, "wide-capture=null-object"),
        "exact failure retained");
Require(Contains(trace, "map-init-association=wide-null-object"),
        "association reports decoder boundary");
```

Require every trace line to begin with one of the first four approved prefixes.

- [ ] **Step 2: Commit/push RED**

```bash
git add tests/FixedMapIdentityProbeTests.cpp
git commit -m "test: specify capture boundary diagnostics"
git push origin main
```

Expected compile failure: the three-argument constructor is absent.

- [ ] **Step 3: Add sink and exact failure names**

In the header add:

```cpp
using TraceSink = std::function<void(std::string)>;
FixedMapIdentityProbe(std::filesystem::path gameRoot,
                      RecordSink recordSink,
                      TraceSink traceSink = {});
```

Add `EmitTrace`, `traceSink_`, `adapterEntered_`, and `firstWideFailure_`. Implement a complete `WideFailureName` switch with:

```text
none, null-object, header-unreadable, length-exceeds-capacity,
length-limit-exceeded, capacity-limit-exceeded, null-storage,
address-overflow, storage-unreadable, missing-terminator, allocation-failure
```

- [ ] **Step 4: Retain bounded failed observations**

Implement the locked core of `Observe` as:

```cpp
if (disabled_) return;
if (adapterEntered_ >= kMaximumRawCaptures) {
    rawOverflow_ = true;
    return;
}
++adapterEntered_;
if (!capture) {
    if (firstWideFailure_ == WideCaptureFailure::None) {
        firstWideFailure_ = capture.failure;
    }
    return;
}
pending_.push_back({std::move(capture), sequence, nowMs});
```

Reset both diagnostic fields with existing epoch state in list change, back, map-init consumption, and disable.

- [ ] **Step 5: Emit classification at map-init**

Emit count, then `wide-capture`, `path-validation`, and exactly one association. Priority: overflow, confirmed, path failure, wide failure, existing state failure. A failed event never hides a confirmed valid event. Use:

```cpp
EmitTrace("adapter-entered=" + std::to_string(adapterEntered));
EmitTrace("wide-capture=" + std::string(WideFailureName(failure)));
EmitTrace("path-validation=" + std::string(PathFailureName(pathFailure)));
EmitTrace("map-init-association=" + association);
```

Omit wide/path lines when that boundary was never reached.

- [ ] **Step 6: Commit/push GREEN and rerun**

```bash
git add src/identity/FixedMapIdentityProbe.h src/identity/FixedMapIdentityProbe.cpp
git commit -m "feat: classify capture diagnostic boundaries"
git push origin main
```

Require two complete GREEN workflow runs.

---

### Task 3: Create the Exclusive Project Trace Sink

**Files:**
- Create: `src/diagnostics/CaptureTrace.h`
- Create: `src/diagnostics/CaptureTrace.cpp`
- Create: `tests/CaptureTraceTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: an existing configured directory and PID.
- Produces: `IsCaptureTraceRootAdmitted`, `Open`, `Write`, `Close`, and `path`; `Write` accepts only five approved prefixes.

- [ ] **Step 1: Write the RED contract test**

Create `tests/CaptureTraceTests.cpp` with the standard local `Require` helper and a unique temporary directory. Exercise this exact contract:

```cpp
CaptureTrace first;
Require(first.Open(root, 4242u), "existing directory admitted");
const auto firstPath = first.path();
Require(firstPath.filename() == L"capture-trace-4242.log",
        "base name uses pid");
Require(first.Write("adapter-entered=1"), "adapter line accepted");
Require(first.Write("wide-capture=null-object"), "wide line accepted");
Require(first.Write("path-validation=success"), "path line accepted");
Require(first.Write("map-init-association=wide-null-object"),
        "association line accepted");
Require(first.Write("controlled-stop-flush=success"),
        "stop line accepted");
Require(!first.Write("module name=S4_Main.exe"),
        "non-allowlisted line rejected");
first.Close();

CaptureTrace second;
Require(second.Open(root, 4242u), "same pid gets unique file");
Require(second.path().filename() == L"capture-trace-4242-1.log",
        "existing trace not overwritten");
second.Close();

Require(ReadAll(firstPath).find("module name=") == std::string::npos,
        "rejected line absent");
CaptureTrace missing;
Require(!missing.Open(root / L"missing", 1u),
        "missing directory not created");
CaptureTrace fileRoot;
Require(!fileRoot.Open(firstPath, 1u), "regular file rejected as root");

Require(IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, true),
        "plain final-resolved directory admitted");
Require(!IsCaptureTraceRootAdmitted(
            FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT, true),
        "reparse directory rejected");
Require(!IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_NORMAL, true),
        "non-directory attributes rejected");
Require(!IsCaptureTraceRootAdmitted(FILE_ATTRIBUTE_DIRECTORY, false),
        "missing final-handle path rejected");
```

Declare/call `RunCaptureTraceTests()` in `tests/TestMain.cpp`; add new test and source to both CMake targets.

- [ ] **Step 2: Commit/push RED**

```bash
git add tests/CaptureTraceTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: specify exclusive capture trace"
git push origin main
```

Expected compile failure: `diagnostics/CaptureTrace.h` missing.

- [ ] **Step 3: Implement the exact public interface**

```cpp
#pragma once
#include <windows.h>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace campaign_completion {
bool IsCaptureTraceRootAdmitted(DWORD attributes,
                                bool finalPathAvailable) noexcept;
class CaptureTrace final {
public:
    bool Open(const std::filesystem::path& root, DWORD processId);
    bool Write(std::string_view record);
    void Close();
    const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::mutex mutex_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
};
}
```

In `CaptureTrace.cpp`, implement `IsCaptureTraceRootAdmitted` as the single
attribute/final-path predicate used by both tests and `Open`. Then:

1. Require root attributes to include directory and exclude reparse point.
2. Open root with `OPEN_EXISTING | FILE_FLAG_BACKUP_SEMANTICS`; require `GetFinalPathNameByHandleW` success.
3. Try base name, then suffixes `-1` through `-99`, with `CreateFileW(..., CREATE_NEW, FILE_ATTRIBUTE_NORMAL)`.
4. Write UTF-8 plus `\r\n` through `WriteFile`; require full byte count and `FlushFileBuffers`.
5. Close each handle once; never reopen by name.

The prefix array is:

```cpp
constexpr std::array<std::string_view, 5> kAllowedPrefixes{
    "adapter-entered=", "wide-capture=", "path-validation=",
    "map-init-association=", "controlled-stop-flush="};
```

Reject CR/LF, `0x`, and case-insensitive `module name=`, `ui-pages`, `mouse`, `victory`, `completion`, `marker`.

- [ ] **Step 4: Commit/push GREEN and rerun**

```bash
git add src/diagnostics/CaptureTrace.h src/diagnostics/CaptureTrace.cpp
git commit -m "feat: add exclusive capture trace sink"
git push origin main
```

Require two GREEN runs and proof the first file stayed byte-identical after the second opened.

---

### Task 4: Wire Configuration and Runtime Lifecycle

**Files:**
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `CaptureTrace`, probe `TraceSink`, and `[Diagnostic] CaptureTraceRoot`.
- Produces: version 0.2.4, optional trace startup, and safe stop flush.

- [ ] **Step 1: Pin RED runtime policy**

Update required INI fields to include `Version=0.2.4` and `CaptureTraceRoot=`. Add:

```cpp
Require(policy.find("CaptureTraceRoot=F:") == std::string::npos,
        "packaged trace root remains empty");
Require(runtime.find("version=0.2.4") != std::string::npos,
        "runtime identifies phase 2.4");
```

Require `fixedMapHook_.Stop()` before `listeners_.Stop()`, and `captureTrace_.Close()` after both.

- [ ] **Step 2: Commit/push RED**

```bash
git add tests/RuntimePolicyTests.cpp
git commit -m "test: specify phase 2.4 runtime policy"
git push origin main
```

Expected CTest failure because source/config still identify 0.2.3.

- [ ] **Step 3: Own and open the optional trace**

Include `diagnostics/CaptureTrace.h` and add `CaptureTrace captureTrace_;` to runtime. After deriving the game directory, read only:

```cpp
const auto campaignDirectory = gameDirectory / L"CampaignCompletion";
const auto iniPath = campaignDirectory / L"CampaignCompletionDebug.ini";
wchar_t traceRoot[32768]{};
GetPrivateProfileStringW(L"Diagnostic", L"CaptureTraceRoot", L"",
                         traceRoot, static_cast<DWORD>(std::size(traceRoot)),
                         iniPath.c_str());
if (traceRoot[0] != L'\0') {
    captureTrace_.Open(traceRoot, GetCurrentProcessId());
}
```

Do not create the configured directory or fail gameplay/runtime if trace opening fails.

- [ ] **Step 4: Wire only probe boundary records**

```cpp
probe_ = std::make_unique<FixedMapIdentityProbe>(
    gameDirectory,
    [this](std::string value) {
        logger_.Write(LogLevel::Info, std::move(value));
    },
    [this](std::string value) {
        captureTrace_.Write(value);
    });
```

Update general header to 0.2.4. Never route module/UI/mouse/GUI records to `CaptureTrace`.

- [ ] **Step 5: Preserve shutdown order and close trace**

Order:

1. disable probe;
2. stop Hook;
3. stop listeners;
4. release S4ModApi;
5. write `controlled-stop-flush=success` only for Hook `None` plus zero listener failures, otherwise `failed`;
6. close trace;
7. close general logger.

No trace method may occur in `FixedMapLoadHook::Dispatch`.

- [ ] **Step 6: Update packaged INI**

Retain all policy fields and use:

```ini
; CampaignCompletionDebug phase-2.4 bounded capture trace settings.
[Diagnostic]
Version=0.2.4
CaptureTraceRoot=
```

- [ ] **Step 7: Commit/push GREEN and rerun**

```bash
git add src/runtime/DiagnosticRuntime.h src/runtime/DiagnosticRuntime.cpp \
        config/CampaignCompletionDebug.ini CMakeLists.txt
git commit -m "feat: integrate bounded capture trace"
git push origin main
```

Require two complete GREEN runs and unchanged two-entry package layout.

---

### Task 5: Strengthen Static Policy Verification

**Files:**
- Modify: `tools/verify_hook_adapter.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Consumes: built ASI and Hook source.
- Produces: CI rejection of hot-path I/O/logging or any extra patch backend.

- [ ] **Step 1: Add RED workflow self-test**

Add optional verifier parameter:

```powershell
[string]$FixedMapHookSource = "src/hook/FixedMapLoadHook.cpp"
```

The workflow copies Hook source to runner temp, injects `WriteFile` inside `Dispatch`, runs the verifier, and fails if the mutated fixture is accepted. Commit only the workflow self-test first.

- [ ] **Step 2: Commit/push RED**

```bash
git add .github/workflows/build-debug-asi.yml
git commit -m "test: require hook hot-path I/O rejection"
git push origin main
```

Expected failure: mutated source unexpectedly accepted.

- [ ] **Step 3: Implement bounded source scan**

Extract source text from `void FixedMapLoadHook::Dispatch` through the adapter declaration. Reject case-insensitively:

```powershell
$forbiddenHotPath = @(
  'logger_', 'CaptureTrace', 'filesystem', 'fstream',
  'CreateFile', 'WriteFile', 'GetPrivateProfile'
)
```

Retain existing export/header/disassembly, `ret 0x0C`, one `make_unique<hlib::CallPatch>`, and no-`JmpPatch` checks.

- [ ] **Step 4: Commit/push GREEN and rerun**

```bash
git add tools/verify_hook_adapter.ps1 .github/workflows/build-debug-asi.yml
git commit -m "ci: reject capture hot-path file I/O"
git push origin main
```

Require mutated fixture rejection, real source acceptance, and two GREEN workflows.

---

### Task 6: Freeze and Verify the 0.2.4 Artifact

**Files:**
- No tracked changes.
- Create ignored files under `dist/phase-2-4/`.

**Interfaces:**
- Consumes: successful workflow artifact whose head SHA equals current `main`.
- Produces: frozen ASI/INI hashes and byte-exact deployment inputs.

- [ ] **Step 1: Verify repository and CI identity**

```bash
git status --short
head_sha="$(git rev-parse HEAD)"
test "$head_sha" = "$(git rev-parse origin/main)"
"/mnt/c/Program Files/GitHub CLI/gh.exe" run list \
  --workflow build-debug-asi.yml --limit 5 \
  --json databaseId,headSha,conclusion,createdAt
```

Expected: clean tracked worktree and two consecutive successful runs for `head_sha`.

- [ ] **Step 2: Download exact successful artifact**

Resolve the newest successful run for HEAD without a placeholder:

```bash
run_id="$("/mnt/c/Program Files/GitHub CLI/gh.exe" run list \
  --workflow build-debug-asi.yml --limit 20 \
  --json databaseId,headSha,conclusion \
  --jq ".[] | select(.headSha == \"$head_sha\" and .conclusion == \"success\") | .databaseId" \
  | head -1)"
test -n "$run_id"
mkdir -p dist/phase-2-4/download dist/phase-2-4/package
"/mnt/c/Program Files/GitHub CLI/gh.exe" run download "$run_id" \
  --name CampaignCompletionDebug-Win32 \
  --dir "$(wslpath -w dist/phase-2-4/download)"
```

Use PowerShell `Expand-Archive` to extract the downloaded ZIP into the ignored package directory. Artifact extraction is mechanical; do not edit binaries.

- [ ] **Step 3: Freeze identity and package policy**

```bash
sha256sum dist/phase-2-4/download/CampaignCompletionDebug.zip \
  dist/phase-2-4/package/Plugins/CampaignCompletionDebug.asi \
  dist/phase-2-4/package/CampaignCompletion/CampaignCompletionDebug.ini
file dist/phase-2-4/package/Plugins/CampaignCompletionDebug.asi
unzip -Z1 dist/phase-2-4/download/CampaignCompletionDebug.zip | sort
```

Expected `pei-i386` and only:

```text
CampaignCompletion/CampaignCompletionDebug.ini
Plugins/CampaignCompletionDebug.asi
```

Run archive integration and ABI verification against the frozen ASI. Any identity, layout, machine, or verifier mismatch is NO-GO.

---

### Task 7: Guarded Deployment and One Live Boundary Test

**Files:**
- Modify only the authorized installed ASI/archive/config paths.
- Create ignored `dist/phase-2-4/runtime/`.

**Interfaces:**
- Consumes: frozen Task 6 ASI/INI.
- Produces: one unique per-process trace for an explicit Single/Aeneas launch.

- [ ] **Step 1: Require user-controlled process shutdown**

Ask the user to close game and SU normally. Never terminate them. Proceed only after read-only process checks find neither `S4_Main` nor `Settlers United`.

- [ ] **Step 2: Verify immutable backup**

Require `research/backups/settlers-united/Plugin_SU.zip.original` SHA-256 to equal:

```text
807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265
```

Record current installed archive hash before replacement.

- [ ] **Step 3: Install only the frozen ASI**

Ask the user to run elevated:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "F:\claude projects\thesettler4plugin\tools\install_settlers_united_artifact.ps1" `
  -AsiPath "F:\claude projects\thesettler4plugin\dist\phase-2-4\package\Plugins\CampaignCompletionDebug.asi"
```

Require installer `originalSha256` to match the immutable original and `embeddedAsiSha256` to match frozen ASI.

- [ ] **Step 4: Deploy only the authorized INI customization**

Create `dist/phase-2-4/runtime/`. Copy frozen INI to authorized game `CampaignCompletion/CampaignCompletionDebug.ini`, then change only:

```ini
CaptureTraceRoot=F:\claude projects\thesettler4plugin\dist\phase-2-4\runtime
```

Do not alter frozen INI. Record deployed hash and prove the only textual difference is this value.

- [ ] **Step 5: Verify installed state before launch**

Require:

- installed ZIP has exactly one `Plugins/CampaignCompletionDebug.asi`;
- embedded ASI hash equals frozen ASI;
- immutable backup hash is unchanged;
- trace directory exists inside project;
- runtime directory contains no trace for the upcoming PID;
- game and SU are not running.

- [ ] **Step 6: Run one user-driven Single/Aeneas test**

User actions:

1. start normally and wait at main menu;
2. enter Single Player Maps;
3. explicitly click calibrated Single tab ID `2449` even if selected;
4. select Aeneas and launch;
5. remain in map and report completion.

Read only the new project trace. Decide:

- `adapter-entered=0` + `map-init-association=no-candidate`: call site not traversed;
- positive count plus any `wide-capture=` value other than `success`: decoder boundary failed;
- `wide-capture=success` plus any `path-validation=` value other than `success`: validation boundary failed;
- all success + confirmed association: capture chain GO.

Do not modify/redeploy while game runs.

- [ ] **Step 7: Controlled stop and responsiveness**

Create only authorized `CampaignCompletionDebug.stop`. Require general log:

```text
fixed-map probe disabled
fixed-map hook stop result=0
listeners stopped registered=75 removed=75 failures=0
diagnostic runtime stopped
```

Require trace ending `controlled-stop-flush=success`; ask user to confirm game response. Never terminate it.

---

### Task 8: Record Evidence and Boundary Decision

**Files:**
- Create: `docs/research/phase-2-4-bounded-capture-trace-report.md`

**Interfaces:**
- Consumes: CI IDs, frozen/deployed hashes, trace, stop result, and user confirmation.
- Produces: auditable conclusion without expanding Hook scope.

- [ ] **Step 1: Write exact report evidence**

Include:

- commit and both successful run/job IDs;
- artifact ID/digest and package/ASI/INI hashes;
- installed archive and embedded ASI hashes;
- immutable backup hash and restore readiness;
- deployed INI hash and sole trace-root difference;
- trace filename, PID, adapter count, wide/path/association results;
- Hook stop result, listener `75/75/0`, responsiveness;
- explicit statement that no completion/victory/persistence/marker work occurred.

Do not include an absolute map path, module inventory, pointer, or memory content.

- [ ] **Step 2: State exactly one conclusion**

Use exactly one:

```text
NO-GO: approved call site was not traversed by the fixed-map launch.
NO-GO: adapter executed but wide capture failed with the recorded reason.
NO-GO: wide capture succeeded but path validation failed with the recorded reason.
GO: adapter, bounded wide capture, path validation, and map-init association succeeded.
```

On NO-GO, do not select another Hook in this task; start a new design/static review. On GO, resume the already designed three-list live sequence without code changes.

- [ ] **Step 3: Verify, commit, and push report**

```bash
git diff --check
git status --short
git add docs/research/phase-2-4-bounded-capture-trace-report.md
git commit -m "docs: report phase 2.4 capture boundary"
git push origin main
test "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)"
```

Before commit, manually scan the report for pointer-like hex and Windows absolute paths. Policy words are allowed only in the explicit scope disclaimer, never as a claimed result.
