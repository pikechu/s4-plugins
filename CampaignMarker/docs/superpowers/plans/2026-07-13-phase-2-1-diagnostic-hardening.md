# Phase 2.1 Diagnostic Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the diagnostic ASI persist through Settlers United synchronization, attribute nested UI pages exactly, and prove safe reverse listener removal in the live game.

**Architecture:** A guarded PowerShell module backs up and patches only Settlers United's `Plugin_SU.zip`. Native fixed-size page aggregation receives a distinct compile-time callback thunk for each S4 GUI page. A control loop consumes an authorized stop-request file, quiesces callbacks, removes handles in reverse order, and releases S4ModApi while leaving the ASI loaded and inert.

**Tech Stack:** C++17, MSVC Win32, Win32 synchronization/filesystem APIs, S4ModApi 2.3.2 public interface, PowerShell 5+, .NET ZIP APIs, CMake/CTest, GitHub Actions.

## Global Constraints

- Target only `S4_Main.exe` file version `2.50.1516.0` with SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Do not add completion detection, storage, marker rendering, MinHook, or internal game hooks.
- Game files remain read-only except project-owned `CampaignCompletion*.asi` and the `CampaignCompletion` directory.
- The only persistent Settlers United file that may be modified is `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
- A temporary sibling named exactly `Plugin_SU.zip.campaigncompletion.tmp` is permitted only during verified replacement and must be removed on success and failure.
- Back up the original archive under `research/backups/settlers-united/` before replacement; never commit the proprietary backup or generated metadata.
- Develop directly on `main`; do not create a worktree or feature branch.
- Follow red-green TDD. Push red test commits and observe the expected GitHub Actions failure before implementation.
- Never deploy or replace the ASI while `S4_Main.exe` is running.

---

## Planned File Structure

- `.gitignore`: generated downloads, scratch extraction, and backup exclusions.
- `tools/SettlersUnitedArtifact.psm1`: archive hashes, backup, patch, verification, and restore.
- `tools/test_settlers_united_artifact.ps1`: dependency-free temporary ZIP tests.
- `tools/install_settlers_united_artifact.ps1`: fixed-path install wrapper.
- `tools/restore_settlers_united_artifact.ps1`: fixed-path restore wrapper.
- `src/runtime/PageObservation.h/.cpp`: fixed-size page aggregation.
- `src/runtime/ListenerRemoval.h/.cpp`: reverse removal policy and counts.
- `src/runtime/StopRequest.h/.cpp`: one-shot stop-file consumption.
- `src/runtime/CallbackGate.h/.cpp`: callback quiescence.
- `src/runtime/S4Listeners.h/.cpp`: per-page thunks and guarded callbacks.
- `src/runtime/DiagnosticRuntime.h/.cpp`: PID header and control loop.
- `tests/PageObservationTests.cpp`: exact page tests.
- `tests/ListenerRemovalTests.cpp`: reverse removal tests.
- `tests/StopRequestTests.cpp`: stop-file tests.
- `tests/RuntimePolicyTests.cpp`: callback gate tests.
- `docs/research/phase-2-1-diagnostic-hardening-report.md`: final evidence.

## Task 1: Settlers United Archive Integration

**Files:**
- Modify: `.gitignore`
- Create: `tools/test_settlers_united_artifact.ps1`
- Create: `tools/SettlersUnitedArtifact.psm1`
- Create: `tools/install_settlers_united_artifact.ps1`
- Create: `tools/restore_settlers_united_artifact.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Produces: `Install-CampaignCompletionArtifact -ArchivePath -AsiPath -BackupDirectory`.
- Produces: `Restore-CampaignCompletionArtifact -ArchivePath -BackupDirectory`.
- Produces: `Plugin_SU.backup.json` containing original/patched/embedded hashes, sizes, timestamps, and source path.

- [ ] **Step 1: Add generated-data exclusions**

Add exactly:

```gitignore
/dist/
/research/tmp/
/research/backups/
```

- [ ] **Step 2: Write failing archive tests**

Create a test script that builds a fresh temporary
`s4_artifacts/Plugin_SU.zip` with
`Plugins/SettlersUnited.asi` and `Plugins/HotkeysConfig/readme.txt`, plus two
different dummy diagnostic ASIs. Define strict assertions:

```powershell
function Assert-Equal($Expected, $Actual, [string]$Message) {
    if ($Expected -ne $Actual) {
        throw "$Message expected=[$Expected] actual=[$Actual]"
    }
}
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
```

The script imports `SettlersUnitedArtifact.psm1` and verifies:

```powershell
$originalHash = Get-FileSha256 $archive
$first = Install-CampaignCompletionArtifact $archive $asiOne $backup
Assert-Equal $originalHash $first.OriginalSha256 'original hash'
Assert-Equal (Get-FileSha256 $asiOne) $first.EmbeddedAsiSha256 'embedded ASI'
Assert-True (Test-Path "$backup/Plugin_SU.zip.original") 'backup missing'
Assert-True (-not (Test-Path "$archive.campaigncompletion.tmp")) 'temp leaked'

$before = Get-ZipEntryHashes "$backup/Plugin_SU.zip.original"
$after = Get-ZipEntryHashes $archive
Assert-Equal $before['Plugins/SettlersUnited.asi'] $after['Plugins/SettlersUnited.asi'] 'existing ASI changed'
Assert-Equal $before['Plugins/HotkeysConfig/readme.txt'] $after['Plugins/HotkeysConfig/readme.txt'] 'text changed'

$second = Install-CampaignCompletionArtifact $archive $asiTwo $backup
Assert-Equal (Get-FileSha256 $asiTwo) $second.EmbeddedAsiSha256 'repatch failed'
Restore-CampaignCompletionArtifact $archive $backup
Assert-Equal $originalHash (Get-FileSha256 $archive) 'restore hash'
```

It must also mutate a patched archive externally and require restore refusal,
simulate failed verification and require the installed hash unchanged, and
remove only its temporary test root in `finally`.

- [ ] **Step 3: Run RED locally and in CI**

Run:

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test_settlers_united_artifact.ps1
```

Expected: import failure because `SettlersUnitedArtifact.psm1` is absent.

Add this workflow step before CMake configuration:

```yaml
- name: Test Settlers United archive integration
  shell: pwsh
  run: ./tools/test_settlers_united_artifact.ps1
```

Commit/push only ignore rules, test, and workflow. Require the same import error:

```bash
git add .gitignore tools/test_settlers_united_artifact.ps1 .github/workflows/build-debug-asi.yml
git commit -m "test: define Settlers United artifact integration"
git push origin main
```

- [ ] **Step 4: Implement archive primitives**

The module begins with:

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-FileSha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-AuthorizedArchivePath([string]$ArchivePath) {
    $full = [IO.Path]::GetFullPath($ArchivePath)
    if ([IO.Path]::GetFileName($full) -cne 'Plugin_SU.zip' -or
        [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($full)) -cne 's4_artifacts') {
        throw "Archive must be s4_artifacts/Plugin_SU.zip: $full"
    }
    $full
}
```

`Get-ZipEntryHashes` opens with `[IO.Compression.ZipFile]::OpenRead`, skips
directory entries, rejects duplicate names, streams each entry through
`SHA256.Create()`, and returns a case-sensitive ordinal dictionary keyed by
forward-slash names.

- [ ] **Step 5: Implement guarded install and restore**

`Install-CampaignCompletionArtifact` derives only:

```powershell
$archive = Assert-AuthorizedArchivePath $ArchivePath
$tempSibling = "$archive.campaigncompletion.tmp"
$backupZip = Join-Path $BackupDirectory 'Plugin_SU.zip.original'
$metadataPath = Join-Path $BackupDirectory 'Plugin_SU.backup.json'
```

It rejects missing inputs. On first use it copies and hash-verifies the original
backup. It extracts the
installed archive to a temporary workspace, replaces only
`Plugins/CampaignCompletionDebug.asi`, rebuilds the ZIP, compares every original
entry hash except an existing diagnostic ASI, rejects missing/duplicate entries,
and verifies the embedded ASI hash.

Only after verification may it copy to `$tempSibling`, reopen/verify that file,
replace `$archive`, and write metadata. A `finally` removes workspace and the
temporary sibling. If replacement changes the installed file and then fails, it
hash-verifies and restores the original backup before rethrowing.

`Restore-CampaignCompletionArtifact` requires backup hash to match
`originalSha256` and installed hash to match `patchedSha256`; otherwise it
refuses. It restores through the same verified temporary sibling.

Export only:

```powershell
Export-ModuleMember -Function Get-FileSha256, Get-ZipEntryHashes,
    Install-CampaignCompletionArtifact, Restore-CampaignCompletionArtifact
```

- [ ] **Step 6: Add fixed-path wrappers and verify GREEN**

The install wrapper accepts `-AsiPath`, optionally accepts the Settlers United
directory defaulting to `C:\Program Files\Settlers United`, and derives the
archive name and repository backup directory itself. Restore derives the same
paths and accepts no arbitrary archive filename. Both wrappers reject execution
while either `S4_Main` or `Settlers United` is running; keeping this guard in the
installed-path wrappers allows temporary sample archives to be tested safely.

Run tests twice, push, and require the PowerShell workflow step to pass:

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test_settlers_united_artifact.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test_settlers_united_artifact.ps1
git add .gitignore tools .github/workflows/build-debug-asi.yml
git commit -m "feat: add guarded Settlers United artifact integration"
git push origin main
```

## Task 2: Exact Per-Page Attribution

**Files:**
- Create: `src/runtime/PageObservation.h`
- Create: `src/runtime/PageObservation.cpp`
- Create: `tests/PageObservationTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`

**Interfaces:**
- Produces: `PageObservationWindow::Observe(DWORD page, uint64_t nowMs)` returning `std::optional<PageSnapshot>`.
- `PageSnapshot.activePages` is sorted/unique; `primaryPage` is the greatest active page.

- [ ] **Step 1: Write the failing page-window test**

```cpp
campaign_completion::PageObservationWindow window(1000);
Require(!window.Observe(4, 0).has_value(), "parent starts window");
Require(!window.Observe(22, 10).has_value(), "child joins window");
Require(!window.Observe(22, 999).has_value(), "duplicate before deadline");
const auto snapshot = window.Observe(22, 1000);
Require(snapshot.has_value(), "deadline produces snapshot");
Require(snapshot->activePages == std::vector<DWORD>({4, 22}), "sorted unique pages");
Require(snapshot->primaryPage == 22, "specific child is primary");
Require(!window.Observe(1, 1001).has_value(), "window resets");
```

Add the test to `TestMain.cpp` and CMake. Commit/push tests only and require CI
failure on missing `runtime/PageObservation.h`.

- [ ] **Step 2: Implement the fixed-size window**

Define:

```cpp
struct PageSnapshot {
    std::vector<DWORD> activePages;
    DWORD primaryPage = S4_GUI_UNKNOWN;
};

class PageObservationWindow final {
public:
    explicit PageObservationWindow(std::uint64_t intervalMs);
    std::optional<PageSnapshot> Observe(DWORD page, std::uint64_t nowMs);
private:
    std::array<bool, S4_GUI_ENUM_MAXVALUE> observed_{};
    std::uint64_t intervalMs_;
    std::uint64_t startedAtMs_ = 0;
    bool started_ = false;
};
```

Ignore page zero/out-of-range. Mark a fixed slot. At the interval, build an
ascending vector, choose `.back()` as primary, then clear array/timing state.

- [ ] **Step 3: Generate one callback thunk per enum**

In `S4Listeners`:

```cpp
template <DWORD Page>
static HRESULT S4HCALL OnUiFrameFor(LPDIRECTDRAWSURFACE7, INT32, LPVOID) {
    DispatchUiFrame(Page);
    return S_OK;
}

template <std::size_t... Index>
static constexpr auto MakeUiCallbacks(std::index_sequence<Index...>) {
    return std::array<LPS4FRAMECALLBACK, sizeof...(Index)>{
        &OnUiFrameFor<static_cast<DWORD>(Index + 1)>...};
}
```

Register callback `page - 1` with the matching enum. Feed exact page values to
the observation window. Log `ui-pages active=4,22 primary=22` and use primary for
mouse/GUI summaries. Remove `IsCurrentlyOnScreen` entirely.

- [ ] **Step 4: Verify GREEN and commit**

Require all tests and ASI link to pass, then commit/push:

```bash
git add src/runtime/PageObservation.* src/runtime/S4Listeners.* tests/PageObservationTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "feat: attribute UI callbacks to exact pages"
git push origin main
```
## Task 3: Reverse Removal and Stop-Request Policies

**Files:**
- Create: `src/runtime/ListenerRemoval.h`
- Create: `src/runtime/ListenerRemoval.cpp`
- Create: `src/runtime/StopRequest.h`
- Create: `src/runtime/StopRequest.cpp`
- Create: `tests/ListenerRemovalTests.cpp`
- Create: `tests/StopRequestTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `ListenerStopResult RemoveListenersInReverse(std::vector<S4HOOK>&, const ListenerRemover&)`.
- Produces: `bool ConsumeStopRequest(const std::filesystem::path&)`.

- [ ] **Step 1: Write failing reverse-removal tests**

Define the desired public contract in the test:

```cpp
struct ListenerStopResult {
    std::size_t registered = 0;
    std::size_t removed = 0;
    std::size_t failures = 0;
};
using ListenerRemover = std::function<HRESULT(S4HOOK)>;
```

Test handles `{10,20,30}` with a remover that records attempts and returns
`S_OK`. Require order `{30,20,10}`, counts `3/3/0`, and an empty vector. A second
test returns `E_FAIL` for handle `20` and requires counts `3/2/1` while still
attempting every handle.

- [ ] **Step 2: Write failing stop-request tests**

Use a unique temporary directory. Require false before the file exists, create
`CampaignCompletionDebug.stop`, require true plus file removal, then require
false again. Clean only the test directory.

- [ ] **Step 3: Push RED**

Wire both tests into `TestMain.cpp` and CMake, commit/push tests only, and require
CI Build failure on the two missing headers:

```bash
git add tests/ListenerRemovalTests.cpp tests/StopRequestTests.cpp tests/TestMain.cpp CMakeLists.txt
git commit -m "test: define controlled stop policies"
git push origin main
```

- [ ] **Step 4: Implement the pure helpers**

`RemoveListenersInReverse` captures starting size, invokes every reverse
iterator, counts `SUCCEEDED`/failure, clears the vector, and returns the result.

`ConsumeStopRequest` uses nonthrowing `std::filesystem::exists` and `remove` with
`std::error_code`; it returns true only when one existing request is removed.

- [ ] **Step 5: Verify GREEN and commit**

```bash
git add src/runtime/ListenerRemoval.* src/runtime/StopRequest.* tests CMakeLists.txt
git commit -m "feat: add controlled stop policies"
git push origin main
```

Require all native tests and plugin link to pass.

## Task 4: Callback Quiescence and Live Control Loop

**Files:**
- Create: `src/runtime/CallbackGate.h`
- Create: `src/runtime/CallbackGate.cpp`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `tests/RuntimePolicyTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Changes: `S4Listeners::Stop()` returns `ListenerStopResult`.
- Produces: `DiagnosticRuntime::RunControlLoop()`.
- Consumes: `CampaignCompletion/CampaignCompletionDebug.stop`.

- [ ] **Step 1: Write failing callback-gate tests**

Add:

```cpp
campaign_completion::CallbackGate gate;
Require(gate.TryEnter(), "running gate accepts callbacks");
gate.Leave();
gate.CloseAndWait();
Require(!gate.TryEnter(), "closed gate rejects callbacks");
```

A second two-thread test enters the gate, starts `CloseAndWait`, proves closure
has not completed, then calls `Leave` and requires closure to complete. Commit
and push this test alone; require missing `CallbackGate` CI failure.

- [ ] **Step 2: Implement callback quiescence**

Use a mutex, condition variable, `accepting_`, and `inFlight_`:

```cpp
bool CallbackGate::TryEnter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) return false;
    ++inFlight_;
    return true;
}

void CallbackGate::Leave() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--inFlight_ == 0) condition_.notify_all();
}

void CallbackGate::CloseAndWait() {
    std::unique_lock<std::mutex> lock(mutex_);
    accepting_ = false;
    condition_.wait(lock, [this] { return inFlight_ == 0; });
}
```

Every public S4 callback loads `active_`, calls `TryEnter`, invokes its observer,
and calls `Leave` through an RAII guard on every exit path.

- [ ] **Step 3: Wire reverse listener removal**

`S4Listeners::Stop` stores `nullptr` in `active_`, closes/waits on the gate, calls
`RemoveListenersInReverse` with `api_->RemoveListener`, nulls API/logger state,
and returns the counts. Registration failure uses the same cleanup path.

- [ ] **Step 4: Add the persistent control loop and PID header**

`DiagnosticRuntime::Start` stores the stop path and builds the header with:

```cpp
std::ostringstream header;
header << "CampaignCompletionDebug bootstrap version=0.2.1 pid="
       << GetCurrentProcessId() << " hook-mode=public-only";
logger_.Write(LogLevel::Info, header.str());
```

`RunControlLoop` checks every 100 ms. On a consumed request it logs
`controlled stop requested`, calls `Stop`, and returns. `Stop` logs:

```cpp
std::ostringstream summary;
summary << "listeners stopped registered=" << result.registered
        << " removed=" << result.removed
        << " failures=" << result.failures;
logger_.Write(LogLevel::Info, summary.str());
logger_.Write(LogLevel::Info, "diagnostic runtime stopped");
```

The bootstrap thread becomes:

```cpp
DWORD WINAPI BootstrapThread(void* module) {
    auto& runtime = RuntimeInstance();
    if (runtime.Start(static_cast<HMODULE>(module))) {
        runtime.RunControlLoop();
    }
    return 0;
}
```

The exported stop function remains. `DllMain` performs no detach cleanup.

- [ ] **Step 5: Verify GREEN and commit**

Require callback-gate tests, all previous tests, and ASI linking to pass:

```bash
git add src/runtime tests/RuntimePolicyTests.cpp CMakeLists.txt
git commit -m "feat: add verifiable controlled listener shutdown"
git push origin main
```

## Task 5: Package Phase 2.1 Artifact

**Files:**
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `README.md`
- Modify: `.github/workflows/build-debug-asi.yml` only if final artifact validation requires correction

**Interfaces:**
- Keeps the deployment ZIP limited to the ASI and diagnostic INI.
- Documents install, restore, backup, and `.stop` operations.

- [ ] **Step 1: Update configuration and documentation**

Set diagnostic version `0.2.1` and add:

```ini
ControlledStopFile=CampaignCompletionDebug.stop
```

README must state that Phase 2.1 intentionally patches the authorized archive
after backup, provide exact install/restore commands, require both applications
closed, and state that backup binaries are ignored by Git.

- [ ] **Step 2: Push and require full workflow success**

Require PowerShell tests, Win32 configure/build, CTest, packaging, PE32 machine
`0x014c`, exact two-file ZIP layout, and artifact upload. Record run ID, artifact
ID/digest, inner ZIP digest, and ASI digest.

- [ ] **Step 3: Download and independently inspect**

Download through the GitHub artifact connector into `dist/phase-2-1/downloaded`,
extract both ZIP layers, require exactly the two expected paths, recompute hashes,
and parse the PE machine independently.

- [ ] **Step 4: Commit documentation/configuration**

```bash
git add README.md config/CampaignCompletionDebug.ini
git commit -m "docs: document phase 2.1 diagnostic controls"
git push origin main
```

## Task 6: Back Up and Patch the Authorized Archive

**Files:**
- Generate, do not commit: `research/backups/settlers-united/Plugin_SU.zip.original`
- Generate, do not commit: `research/backups/settlers-united/Plugin_SU.backup.json`
- Authorized external modification: `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`

**Interfaces:**
- Consumes: Phase 2.1 CI ASI.
- Produces: verified patched archive and verified restorable original.

- [ ] **Step 1: Require the user to close both applications**

Use `Get-Process` to verify neither `S4_Main` nor `Settlers United` is running.
Do not proceed otherwise.

- [ ] **Step 2: Run the guarded installer**

```powershell
$asi = (Resolve-Path '.\dist\phase-2-1\downloaded\package\Plugins\CampaignCompletionDebug.asi').Path
./tools/install_settlers_united_artifact.ps1 -AsiPath $asi
```

Expected: backup precedes replacement; original, patched, and embedded hashes
are printed; no temporary sibling remains.

- [ ] **Step 3: Independently audit archive and backup**

List both ZIPs, compare every original entry hash, verify one diagnostic entry,
verify metadata hashes, and perform a non-mutating restore-readiness check.

- [ ] **Step 4: Launch normally without the old watcher**

Ask the user to launch via Settlers United and remain at main menu. Require the
ASI to survive synchronization, load into the formal process, log version/PID,
and emit no error/warn.

## Task 7: Live Page and Controlled-Stop Acceptance

**Files:**
- Create: `docs/research/phase-2-1-diagnostic-hardening-report.md`

**Interfaces:**
- Consumes: live log, CI evidence, archive metadata, protected manifest.
- Produces: Phase 2.1 GO/NO-GO decision.

- [ ] **Step 1: Exercise exact nested pages**

Ask the user to visit Single Player Maps, Multiplayer Maps, and Custom Maps for
at least three seconds each. Require exact child thunk values and every active
parent in each `ui-pages active=... primary=...` record. Record transitions,
representative element IDs/rectangles, and any page still absent.

- [ ] **Step 2: Trigger controlled stop**

Create only
`F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\CampaignCompletion\CampaignCompletionDebug.stop`.
Require
one consumed request and one summary with `registered > 0`,
`removed == registered`, and `failures == 0`.

- [ ] **Step 3: Prove silence and stability**

Record stop-summary line/file size. Ask the user to navigate main menu → Single
Player → main menu for at least five seconds. Require no later `ui-pages`,
`mouse`, `gui-elements`, or `map-init` record and require `S4_Main.exe` to remain
responsive.

- [ ] **Step 4: Verify integrity and restore readiness**

Run `sha256sum -c research/evidence/manifest.sha256` and require all 12 `OK`.
Require installed archive equals recorded patched hash, backup equals original
hash, embedded ASI equals CI ASI, and restore readiness accepts current state.

- [ ] **Step 5: Write report and decide**

Separate static, CI, archive, runtime, user-observed, and unverified findings.
Internal-probe planning becomes `GO` only if persistent normal launch, exact
page attribution, zero-failure controlled stop, post-stop silence, stability,
and integrity all pass.

- [ ] **Step 6: Fresh verification, commit, and push**

Run `git diff --check`, archive tests, latest CI status, protected manifest, and
report assertions against the live log. Then:

```bash
git add docs/research/phase-2-1-diagnostic-hardening-report.md
git commit -m "docs: report phase 2.1 diagnostic hardening"
git push origin main
```
