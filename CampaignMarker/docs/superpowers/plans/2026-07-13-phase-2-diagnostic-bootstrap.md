# Campaign Completion Phase 2 Diagnostic Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and game-test a Win32 `CampaignCompletionDebug.asi` that performs deferred initialization, inventories loaded modules from inside the game, initializes S4ModApi 2.x, and logs page/map/UI diagnostic events without installing internal game Hooks or writing completion data.

**Architecture:** A minimal DLL entry point starts one bootstrap thread. The bootstrap owns a thread-safe logger, collects executable/module identities through documented Windows APIs, waits for S4ModApi, creates its public COM-style interface, and registers only S4ModApi-managed listeners. GitHub Actions `windows-2022` supplies the missing local MSVC v143 x86 toolchain and publishes a ZIP artifact for authorized deployment.

**Tech Stack:** C++17, Win32, MSVC v143 Win32, CMake 3.24+, S4ModApi v2.3 public header/import library, Windows CryptoAPI for SHA-256, GitHub Actions, Visual Studio C++ test executable.

## Global Constraints

- Build only Win32/x86; fail configuration when `CMAKE_SIZEOF_VOID_P` is not 4.
- Target only `S4_Main.exe` 2.50.1516.0 with SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Require S4ModApi 2.x and dynamically wait for `S4ModApi.dll`; do not load or replace it.
- `DllMain` may call only `DisableThreadLibraryCalls` and create the bootstrap thread; no logging, JSON, Hook installation, waiting, or S4ModApi initialization in loader lock.
- Do not install MinHook or directly patch `S4_Main.exe` in this plan.
- Do not draw UI, read/write completion JSON, classify victory, multiplayer, or random maps in this plan.
- Runtime writes are restricted to `CampaignCompletion/CampaignCompletion.log` created by this plugin.
- Deployment may add/replace only `Plugins/CampaignCompletionDebug.asi` and the `CampaignCompletion` directory.
- Every callback must be non-blocking and rate-limited; file I/O is serialized by the logger.
- Vendor S4ModApi v2.3 files with their LGPL-3.0 notice and pinned upstream commit.

---

## Planned File Structure

- `CMakeLists.txt`: Win32 plugin and native test targets.
- `cmake/VerifyWin32.cmake`: rejects non-x86 configuration.
- `third_party/S4ModApi/S4ModApi.h`: pinned public v2.3 header.
- `third_party/S4ModApi/S4ModApi.lib`: pinned x86 import library.
- `third_party/S4ModApi/NOTICE.md`: source commit, upstream URL, and LGPL notice.
- `src/diagnostics/Logger.h/.cpp`: synchronized UTF-8 line logger.
- `src/diagnostics/ModuleInventory.h/.cpp`: executable version/hash and in-process module enumeration.
- `src/runtime/DiagnosticRuntime.h/.cpp`: deferred lifecycle and S4ModApi ownership.
- `src/runtime/S4Listeners.h/.cpp`: public S4ModApi callbacks and rate limiting.
- `src/dllmain.cpp`: minimal DLL entry point.
- `tests/LoggerTests.cpp`: path and line-format tests.
- `tests/ModuleInventoryTests.cpp`: current-process module inventory and hashing tests.
- `tests/RuntimePolicyTests.cpp`: exact executable allow-list and callback rate-limit tests.
- `.github/workflows/build-debug-asi.yml`: MSVC x86 build, tests, PE verification, and artifact upload.
- `config/CampaignCompletionDebug.ini`: bootstrap diagnostic defaults.
- `tools/package_debug.ps1`: deterministic diagnostic ZIP layout.

## Task 1: Establish the Win32 Build and Test Contract

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/VerifyWin32.cmake`
- Create: `third_party/S4ModApi/NOTICE.md`
- Create: `third_party/S4ModApi/S4ModApi.h`
- Create: `third_party/S4ModApi/S4ModApi.lib`
- Create: `.github/workflows/build-debug-asi.yml`

**Interfaces:**
- Consumes: S4ModApi commit `bb5d40ead1bb2a111ec0f787a6da5c15e130684e`.
- Produces: `CampaignCompletionDebug` shared-library target and `campaign_completion_tests` executable target, both Win32.

- [ ] **Step 1: Add a CI configuration that initially fails because CMake files are absent**

Create `.github/workflows/build-debug-asi.yml` with a manual trigger and these build commands:

```yaml
name: build-debug-asi
on:
  workflow_dispatch:
  push:
    paths:
      - "CMakeLists.txt"
      - "cmake/**"
      - "src/**"
      - "tests/**"
      - "third_party/**"
      - ".github/workflows/build-debug-asi.yml"
jobs:
  build:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - name: Configure Win32
        run: cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Debug
      - name: Build
        run: cmake --build build --config Debug --parallel
      - name: Test
        run: ctest --test-dir build -C Debug --output-on-failure
      - name: Verify PE32
        shell: pwsh
        run: |
          $asi = "build/Debug/CampaignCompletionDebug.asi"
          if (-not (Test-Path $asi)) { throw "ASI missing" }
          $bytes = [IO.File]::ReadAllBytes($asi)
          $pe = [BitConverter]::ToInt32($bytes, 0x3c)
          $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
          if ($machine -ne 0x14c) { throw "Expected IMAGE_FILE_MACHINE_I386" }
      - uses: actions/upload-artifact@v4
        with:
          name: CampaignCompletionDebug-Win32
          path: dist/CampaignCompletionDebug.zip
```

Push only after the remaining Task 1 files exist; the red condition is verified locally with `cmake -S . -B build -A Win32`, expected to fail because no Windows generator/toolchain is installed locally.

- [ ] **Step 2: Vendor the exact S4ModApi ABI files**

Copy `S4ModApi.h` and `Examples/HelloWorld/HelloWorld/S4ModApi.lib` from the pinned research clone into `third_party/S4ModApi`. `NOTICE.md` must record the URL, full commit, v2.3 tag, LGPL-3.0 license, and state that the files are unmodified.

- [ ] **Step 3: Add the Win32 guard**

Create `cmake/VerifyWin32.cmake`:

```cmake
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "CampaignCompletionDebug requires MSVC on Windows")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "CampaignCompletionDebug must be configured with -A Win32")
endif()
```

- [ ] **Step 4: Define build targets**

`CMakeLists.txt` must set C++17, include `VerifyWin32.cmake`, build a shared library with output suffix `.asi`, link `S4ModApi.lib`, `Version.lib`, `Crypt32.lib`, and `Psapi.lib`, enable `/W4 /WX /permissive-`, and register the native test executable with CTest. Set `/MTd` for Debug and `/MT` for Release through `MSVC_RUNTIME_LIBRARY`.

- [ ] **Step 5: Commit the build contract**

```bash
git add CMakeLists.txt cmake third_party .github/workflows/build-debug-asi.yml
git commit -m "build: establish Win32 diagnostic plugin targets"
```

## Task 2: Implement and Test Diagnostic Logging

**Files:**
- Create: `src/diagnostics/Logger.h`
- Create: `src/diagnostics/Logger.cpp`
- Create: `tests/LoggerTests.cpp`

**Interfaces:**
- Produces: `bool Logger::Open(const std::filesystem::path&)`, `void Logger::Write(LogLevel, std::string_view)`, `void Logger::Close()`, and `std::string FormatLogLine(LogLevel, std::string_view, const SYSTEMTIME&)`.

- [ ] **Step 1: Write failing logger tests**

Test that `FormatLogLine(LogLevel::Info, "loaded")` contains an ISO-8601 local timestamp, `[INFO]`, and `loaded`; test that opening a nested temporary path creates its parent directory; and test that two writes produce two complete newline-terminated records.

- [ ] **Step 2: Run CI and confirm the logger tests fail to compile**

Run the workflow with `gh workflow run build-debug-asi.yml --ref main`, then `gh run watch --exit-status`. Expected: build failure naming missing `Logger` symbols.

- [ ] **Step 3: Implement the minimal synchronized logger**

Use `std::mutex`, `std::ofstream`, `std::filesystem::create_directories`, `GetLocalTime`, and UTF-8 text. `Write` returns immediately when the file is not open and flushes every diagnostic line so crash evidence is retained.

- [ ] **Step 4: Re-run CI**

Expected: logger tests pass; plugin target may still fail for runtime files introduced in later tasks only if CMake does not yet list them.

- [ ] **Step 5: Commit**

```bash
git add src/diagnostics/Logger.h src/diagnostics/Logger.cpp tests/LoggerTests.cpp CMakeLists.txt
git commit -m "feat: add diagnostic logger"
```

## Task 3: Implement and Test In-Process Module Inventory

**Files:**
- Create: `src/diagnostics/ModuleInventory.h`
- Create: `src/diagnostics/ModuleInventory.cpp`
- Create: `tests/ModuleInventoryTests.cpp`

**Interfaces:**
- Produces: `std::vector<ModuleInfo> EnumerateLoadedModules()`, `std::optional<std::string> Sha256File(path)`, `std::string FileVersion(path)`, and `CompatibilityResult CheckTargetExecutable(const ModuleInfo&)`.

- [ ] **Step 1: Write failing inventory tests**

Tests must assert that current-process enumeration contains the test executable and `kernel32.dll`, SHA-256 of a temporary file containing `abc` equals `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`, and a non-approved hash returns `CompatibilityResult::HashMismatch`.

- [ ] **Step 2: Run CI and observe missing inventory symbols**

Expected: test compilation or link failure naming `EnumerateLoadedModules` and `Sha256File`.

- [ ] **Step 3: Implement using documented Windows APIs**

Use `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId())`, `Module32FirstW/Module32NextW`, `GetFileVersionInfoSizeW/GetFileVersionInfoW/VerQueryValueW`, and CryptoAPI SHA-256. Sort modules case-insensitively by full path and log name, base address, size, path, version, and hash for `S4_Main.exe`, `S4ModApi.dll`, and all `.asi` modules.

- [ ] **Step 4: Re-run CI and require all inventory tests to pass**

- [ ] **Step 5: Commit**

```bash
git add src/diagnostics/ModuleInventory.h src/diagnostics/ModuleInventory.cpp tests/ModuleInventoryTests.cpp CMakeLists.txt
git commit -m "feat: add in-process module inventory"
```

## Task 4: Implement Deferred Runtime and S4ModApi Listeners

**Files:**
- Create: `src/runtime/DiagnosticRuntime.h`
- Create: `src/runtime/DiagnosticRuntime.cpp`
- Create: `src/runtime/S4Listeners.h`
- Create: `src/runtime/S4Listeners.cpp`
- Create: `src/dllmain.cpp`
- Create: `tests/RuntimePolicyTests.cpp`

**Interfaces:**
- Produces: `DWORD WINAPI BootstrapThread(void*)`, `DiagnosticRuntime::Start(HMODULE)`, `DiagnosticRuntime::Stop()`, and callback functions matching S4ModApi typedefs.
- Consumes: `Logger`, module inventory, `S4ApiCreate`, `AddUIFrameListener`, `AddMapInitListener`, `AddMouseListener`, and `AddGuiElementBltListener`.

- [ ] **Step 1: Write failing policy tests**

Test a pure `RateLimiter` with a 1000 ms interval: first event allowed, event at 999 ms rejected, event at 1000 ms allowed. Test that exact approved EXE hash passes and a one-character mutation fails closed.

- [ ] **Step 2: Implement the minimal DLL entry point**

```cpp
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, BootstrapThread, module, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
```

Do not perform detach cleanup from loader lock. The runtime registers an `atexit`-independent stop path only after successful initialization and relies on process teardown for final OS handle cleanup in this diagnostic version.

- [ ] **Step 3: Implement deferred initialization**

`BootstrapThread` must compute the ASI directory, open `CampaignCompletion/CampaignCompletion.log`, log plugin/build versions, inventory modules, validate the EXE hash, wait up to 30 seconds for `GetModuleHandleW(L"S4ModApi.dll")`, call `S4ApiCreate`, and stop without listeners on any failure.

- [ ] **Step 4: Register only public listeners**

Register:

- one `AddMapInitListener` callback;
- `AddUIFrameListener` for every `S4_GUI_ENUM` value from 1 to `S4_GUI_ENUM_MAXVALUE - 1`;
- one `AddMouseListener` callback for selected/hovered element samples;
- one `AddGuiElementBltListener` callback with one-second per-page summary rate limiting.

Callbacks enqueue or aggregate fixed-size diagnostic records; they must not hash files, enumerate modules, allocate unbounded containers, or flush the log per GUI element.

- [ ] **Step 5: Remove registered listeners on controlled stop**

Store every nonzero `S4HOOK`, call `RemoveListener` in reverse order, then `Release` the API interface. Log failures without retry loops.

- [ ] **Step 6: Run CI and require all policy tests plus plugin link to pass**

- [ ] **Step 7: Commit**

```bash
git add src/runtime src/dllmain.cpp tests/RuntimePolicyTests.cpp CMakeLists.txt
git commit -m "feat: add hook-free diagnostic runtime"
```

## Task 5: Package and Publish the Diagnostic Artifact

**Files:**
- Create: `config/CampaignCompletionDebug.ini`
- Create: `tools/package_debug.ps1`
- Modify: `.github/workflows/build-debug-asi.yml`
- Modify: `README.md`

**Interfaces:**
- Produces: `dist/CampaignCompletionDebug.zip` containing `Plugins/CampaignCompletionDebug.asi` and `CampaignCompletion/CampaignCompletionDebug.ini`.

- [ ] **Step 1: Add packaging verification before implementation**

Workflow must fail unless the ZIP contains exactly the two required deployment files and `CampaignCompletionDebug.asi` is machine `0x14c`.

- [ ] **Step 2: Implement deterministic packaging**

`package_debug.ps1` deletes only `dist/staging`, creates the two target directories, copies the built ASI/config, and calls `Compress-Archive`. It never accepts the game directory as an argument.

- [ ] **Step 3: Document diagnostic-only status**

README must state that this artifact records diagnostics only, does not detect or save completion, does not render markers, and should be removed by deleting only `Plugins/CampaignCompletionDebug.asi` and `CampaignCompletion/CampaignCompletion.log` if desired.

- [ ] **Step 4: Run and watch CI, then download the artifact**

```bash
gh.exe workflow run build-debug-asi.yml --ref main
gh.exe run watch --exit-status
gh.exe run download --name CampaignCompletionDebug-Win32 --dir dist/downloaded
```

Expected: workflow success, all tests pass, PE32 check passes, and one ZIP downloads.

- [ ] **Step 5: Commit**

```bash
git add config/CampaignCompletionDebug.ini tools/package_debug.ps1 .github/workflows/build-debug-asi.yml README.md
git commit -m "build: package diagnostic ASI artifact"
```

## Task 6: Authorized Deployment and Game Test

**Files:**
- Create after test: `docs/research/phase-2-diagnostic-bootstrap-report.md`
- Runtime authorized paths only: `Plugins/CampaignCompletionDebug.asi`, `CampaignCompletion/CampaignCompletionDebug.ini`, and plugin-generated `CampaignCompletion/CampaignCompletion.log`.

**Interfaces:**
- Consumes: verified CI ZIP.
- Produces: game-test evidence and go/no-go decision for individual internal probes.

- [ ] **Step 1: Record pre-deployment hashes**

Run `collect_pe_metadata.sh` and record the existing protected input manifest. Confirm only authorized CampaignCompletion paths will change.

- [ ] **Step 2: Deploy only authorized files**

Extract to a workspace staging directory, verify ZIP paths, then copy the ASI to `Plugins/CampaignCompletionDebug.asi` and config to `CampaignCompletion/CampaignCompletionDebug.ini`. Do not remove or replace any other game file.

- [ ] **Step 3: Launch through Settlers United and remain at main menu for two minutes**

Expected log evidence:

- bootstrap thread started outside loader lock;
- exact EXE version/hash accepted;
- complete module inventory including S4ModApi and installed ASIs;
- S4ModApi interface created;
- main-menu page listener observed;
- no internal Hook installation line;
- no crash or forced exit.

- [ ] **Step 4: Perform user-assisted menu traversal**

Visit Single Player, fixed-map list, Dark Tribe, Three Races, New World, Trojans, and return to main menu. The report must list observed `S4_GUI_ENUM` values, GUI element counts, representative rectangles/IDs, and pages with no callback.

- [ ] **Step 5: Perform one fixed-map load and return without victory**

Expected: one map-init event, no completion JSON, no completion claim, and safe return to menu.

- [ ] **Step 6: Verify protected inputs are unchanged**

Run `sha256sum -c research/evidence/manifest.sha256`. Expected: all 12 protected inputs report `OK`; only authorized plugin/config/log paths may differ.

- [ ] **Step 7: Write and commit the diagnostic report**

Report static, CI, runtime, user-observed, and unverified findings separately. Internal probe planning is `GO` only if module inventory, S4ModApi initialization, listener cleanup, and baseline stability all pass.

```bash
git add docs/research/phase-2-diagnostic-bootstrap-report.md
git commit -m "docs: report phase 2 diagnostic bootstrap"
```
