# Phase 2.2 Read-Only Map Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Distinguish the three fixed-map lists and obtain one stable fixed-map identity before and after `map-init` using verified read-only memory access and existing S4ModApi callbacks only.

**Architecture:** A hash-gated executable layout admits only statically evidenced RVAs. A bounded Win32 reader and map-path validator turn one reviewed object chain into immutable identity observations; a separate UI attribution state machine combines the fixed-map page set with calibrated click controls. `S4Listeners` supplies public events, while the probe emits only state changes and fails closed to `unknown`.

**Tech Stack:** C++17, MSVC Win32/x86, Win32 `VirtualQuery` and filesystem APIs, S4ModApi 2.3.2 public listeners, GNU `objdump` for offline PE analysis, CMake/CTest, PowerShell 5+, GitHub Actions.

## Global Constraints

- Target only `S4_Main.exe` file version `2.50.1516.0` with SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
- Read process memory only; do not write game memory, invoke an internal game function, install a direct hook, patch machine code, or scan the complete heap.
- Continue using only the public S4ModApi listeners already verified in Phase 2.1.
- Do not add campaign missions, random-map identity, multiplayer session state, victory detection, completion storage, or marker rendering.
- Game and Settlers United files remain read-only except project-owned `CampaignCompletion*.asi`, the `CampaignCompletion` configuration directory, and the already authorized guarded replacement of `C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
- Preserve `research/backups/settlers-united/Plugin_SU.zip.original` exactly; never commit the proprietary backup or generated backup metadata.
- Never deploy or replace the ASI while `S4_Main.exe` or Settlers United is running, and never terminate either process automatically.
- Develop directly on `main`; do not create a worktree or feature branch.
- Follow red-green TDD. Push each red test commit, observe the expected GitHub Actions failure, then push the minimal green implementation.
- A failed layout, read, invariant, or identity comparison produces `unknown`; it must not enable a fallback hook or broader scan.

## CI TDD Protocol

This workspace has no local MSVC toolchain. Do not claim a local Win32 build or
CTest result. Every C++ RED/GREEN step uses the Windows GitHub Actions runner.
After each push, identify and watch the run with:

```bash
run_id="$(gh run list --workflow build-debug-asi.yml --branch main --limit 1 \
  --json databaseId --jq '.[0].databaseId')"
gh run watch "$run_id" --exit-status
```

For RED, `gh run watch` must exit nonzero at the named missing header/symbol or
new assertion, while the archive test still passes. For GREEN it must exit zero
after archive test, Win32 configure/build, complete CTest, PE/package check, and
artifact upload all pass. Record every RED and GREEN run ID in the phase report.

---

## Planned File Structure

- `research/scripts/collect_map_identity_candidates.sh`: hash-gated section conversion and bounded disassembly capture.
- `research/evidence/map-identity-candidates.txt`: normalized, reproducible static-analysis output.
- `docs/research/phase-2-2-static-map-identity-evidence.md`: reviewed instruction operands, object chain, and accept/reject decision.
- `src/identity/ExecutableLayout.h/.cpp`: approved image/section model and evidence-owned RVA resolution.
- `src/identity/SafeMemoryReader.h/.cpp`: bounded `VirtualQuery`-validated reads.
- `src/identity/MapPathValidator.h/.cpp`: ANSI decoding, lexical containment, extension, and file checks.
- `src/identity/ListAttribution.h/.cpp`: fixed-map page presence plus calibrated click attribution.
- `src/identity/MapIdentityObservation.h`: immutable observation vocabulary and formatter declarations.
- `src/identity/MapIdentityState.h/.cpp`: menu/loaded session comparison and change suppression.
- `src/identity/VerifiedMapIdentityReader.h/.cpp`: the single statically reviewed pointer chain and its invariants.
- `src/identity/MapIdentityProbe.h/.cpp`: public-event orchestration and diagnostic record production.
- `tests/ExecutableLayoutTests.cpp`, `tests/SafeMemoryReaderTests.cpp`,
  `tests/MapPathValidatorTests.cpp`, `tests/ListAttributionTests.cpp`,
  `tests/MapIdentityStateTests.cpp`, `tests/VerifiedMapIdentityReaderTests.cpp`,
  and `tests/MapIdentityProbeTests.cpp`: focused unit and Win32 tests.
- `src/runtime/S4Listeners.h/.cpp`: forwards page snapshots, click events, and `map-init` to the probe.
- `src/runtime/DiagnosticRuntime.h/.cpp`: constructs the probe only after executable compatibility succeeds.
- `CMakeLists.txt`, `tests/TestMain.cpp`: source/test registration.
- `config/CampaignCompletionDebug.ini`: phase/version and explicit no-hook policy.
- `docs/research/phase-2-2-read-only-map-identity-report.md`: CI, deployment, live capture, hashes, stop proof, and decision.

## Task 1: Static Evidence Gate

**Files:**
- Create: `research/scripts/collect_map_identity_candidates.sh`
- Create: `research/evidence/map-identity-candidates.txt`
- Create: `docs/research/phase-2-2-static-map-identity-evidence.md`

**Interfaces:**
- Consumes: the approved installed `S4_Main.exe`, `S4_MainR.pdb`, `research/evidence/pdb-symbols.txt`, and PE section headers.
- Produces: one accepted `MapObjectLayout` evidence row containing root-pointer RVA, each dereference offset, CMapFile vtable RVA, path storage kind/offset, optional internal-ID offset, and the exact supporting instructions.
- Gate: if no row satisfies all evidence rules, skip Tasks 2-4 and 6-8, execute
  the public-UI-only Task 5, and finish Task 9 as `PARTIAL/NO-GO`; no internal
  reader may be compiled from a rejected chain.

- [ ] **Step 1: Add the hash-gated collection script**

The script must source `research/scripts/common.sh`, require the EXE/PDB/symbol file, verify these fixed hashes from `manifest.sha256`, and refuse any mismatch:

```bash
readonly EXE="$GAME_DIR/S4_Main.exe"
readonly PDB="$GAME_DIR/S4_MainR.pdb"
readonly SYMBOLS="$EVIDENCE_DIR/pdb-symbols.txt"
readonly APPROVED_EXE_SHA256="3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816"

actual_exe_hash="$(sha256sum "$EXE" | awk '{print $1}')"
[[ "$actual_exe_hash" == "$APPROVED_EXE_SHA256" ]] || {
    printf 'EXE hash mismatch: %s\n' "$actual_exe_hash" >&2
    exit 1
}
grep -F "$actual_exe_hash  $EXE" "$EVIDENCE_DIR/manifest.sha256" >/dev/null
```

Record `objdump -h`, the exact PDB records for `S4::CMapFile::sub_B11B0`, `CMapFile@S4` RTTI/vtable, `SetupSingleplayerLobbyMenu`, and `CStateLobbyMapSettings`, and bounded disassembly for the complete containing functions. Convert CodeView `section:offset` with `RVA = section.VirtualAddress + offset`; add image base `0x00400000` only for `objdump --start-address/--stop-address` VMAs. The script writes only `research/evidence/map-identity-candidates.txt` and never touches the game directory.

- [ ] **Step 2: Run the collector twice and prove deterministic payload**

Run:

```bash
bash research/scripts/collect_map_identity_candidates.sh
cp research/evidence/map-identity-candidates.txt /tmp/map-identity-candidates.first
bash research/scripts/collect_map_identity_candidates.sh
diff -u <(sed '/^generated_utc=/d;/^workspace_commit=/d' /tmp/map-identity-candidates.first) \
        <(sed '/^generated_utc=/d;/^workspace_commit=/d' research/evidence/map-identity-candidates.txt)
```

Expected: exit `0`; the normalized diff is empty. Confirm that all disassembly addresses lie in `.text`, the vtable lies in `.rdata`, and candidate root storage lies in `.data`.

- [ ] **Step 3: Write the reviewed static evidence decision**

For every candidate, the report table must contain these concrete columns:

```text
symbol | symbol RVA | containing function range | instruction VMA/RVA |
instruction bytes | decoded operand | target section | inferred ownership |
runtime invariant | accepted/rejected reason
```

The accepted-chain section must include a fully numeric C++ `MapObjectLayout`
initializer. Every root, vtable, dereference, path, and optional ID field must be
a decimal or hexadecimal literal derived from the table; the `PathStorage`
enumerator must be the one demonstrated by the reviewed instruction sequence.
Symbolic field names or explanatory comments in place of numeric values fail the
gate.

Only include offsets actually present in reviewed operands. Reject a guessed class member, an ambiguous global, a path discovered only by string proximity, or a chain requiring an internal call. State whether the menu and post-init samples use the same root or two separately evidenced roots.

- [ ] **Step 4: Commit and push the evidence gate**

Run:

```bash
git diff --check
git add research/scripts/collect_map_identity_candidates.sh \
  research/evidence/map-identity-candidates.txt \
  docs/research/phase-2-2-static-map-identity-evidence.md
git commit -m "research: establish map identity read evidence"
git push origin main
```

Expected: the commit contains no proprietary binary or raw memory dump. Continue only when the report decision is `ACCEPTED FOR READ-ONLY PROBE`.

## Task 2: Approved Executable Layout

**Files:**
- Create: `src/identity/ExecutableLayout.h`
- Create: `src/identity/ExecutableLayout.cpp`
- Create: `tests/ExecutableLayoutTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ModuleInfo`, the accepted Task 1 `MapObjectLayout`, and the live PE image base.
- Produces: `ExecutableLayout::Create(const ModuleInfo&, const MapObjectLayout&)`,
  `ExecutableLayout::FromSectionsForTesting(const ModuleInfo&,
  std::vector<ImageSection>, const MapObjectLayout&)`, and
  `Resolve(const EvidenceRva&)`.
- Produces types: `ImageSection`, `SectionKind`, `EvidenceRva`, `PathStorage`, `MapObjectLayout`, `LayoutFailure`.

- [ ] **Step 1: Push RED layout tests**

Define and test this public shape:

```cpp
enum class SectionKind { Code, ReadOnlyData, WritableData };
struct EvidenceRva final {
    std::uint32_t value;
    SectionKind expectedSection;
    const char* evidence;
};
struct MapObjectLayout final {
    EvidenceRva rootPointer;
    EvidenceRva cMapFileVtable;
    std::array<std::uint32_t, 4> dereferenceOffsets{};
    std::size_t dereferenceCount = 0;
    std::uint32_t pathOffset = 0;
    PathStorage pathStorage = PathStorage::InlineAnsi;
    std::optional<std::uint32_t> internalIdOffset;
};
```

Use `FromSectionsForTesting` with a synthetic `ModuleInfo` whose base is
`0x00400000`, size is `0x012bf000`, version/hash are approved, and sections
match `.text`, `.rdata`, and `.data`. Require valid root/vtable resolution and
reject wrong hash, wrong expected section, RVA equal to image size, `base + rva`
overflow, missing evidence text, and `dereferenceCount > 4`. Tests must never
dereference the synthetic base.

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/ExecutableLayoutTests.cpp
git commit -m "test: define approved executable layout"
git push origin main
```

Expected CI failure: missing `identity/ExecutableLayout.h`.

- [ ] **Step 2: Implement minimal layout validation**

`ExecutableLayout::Create` must first call `CheckTargetExecutable`, open
`ModuleInfo.path` read-only, and parse PE32 section headers from the executable
file rather than dereferencing the live image. Reject malformed `MZ`/`PE\0\0`,
PE32+, truncated headers, and oversized section tables, and retain only these
characteristics:

```cpp
constexpr DWORD kCode = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
constexpr DWORD kReadOnlyData = IMAGE_SCN_MEM_READ;
constexpr DWORD kWritableData = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
```

`Resolve` uses checked addition, requires the complete span to fit one admitted section, and checks section kind against `EvidenceRva.expectedSection`. It returns `std::optional<std::uintptr_t>` plus a stable `LayoutFailure` reason; it never dereferences the resolved address.

- [ ] **Step 3: Verify GREEN and push**

Push and run the CI TDD protocol:

```bash
git add CMakeLists.txt src/identity/ExecutableLayout.* tests/ExecutableLayoutTests.cpp tests/TestMain.cpp
git commit -m "feat: add hash-gated executable layout"
git push origin main
```

Expected: all CTest tests pass; GitHub Win32 build passes.

## Task 3: Bounded Win32 Memory Reader

**Files:**
- Create: `src/identity/SafeMemoryReader.h`
- Create: `src/identity/SafeMemoryReader.cpp`
- Create: `tests/SafeMemoryReaderTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `ReadBytes(std::uintptr_t, void*, std::size_t)`, `ReadPointer(std::uintptr_t)`, `ReadU32(std::uintptr_t)`, and `ReadCString(std::uintptr_t, std::size_t)`.
- Every method returns `MemoryReadResult<T>` containing
  `std::optional<T> value` and `MemoryReadFailure failure`; success is exactly
  `value.has_value()` with `failure == MemoryReadFailure::None`. No exception
  crosses the API.

- [ ] **Step 1: Push RED Win32 reader tests**

Allocate pages with `VirtualAlloc` and require:

```cpp
SafeMemoryReader reader(512);
Require(reader.ReadU32(readableAddress).value == 0x12345678, "read u32");
Require(reader.ReadCString(stringAddress, 16).value == "Map\\User\\A.map", "read c string");
Require(reader.ReadBytes(0, buffer, 4).failure == MemoryReadFailure::NullAddress, "null");
Require(reader.ReadBytes(readableAddress, buffer, 513).failure == MemoryReadFailure::Oversized, "cap");
Require(reader.ReadBytes(noAccessAddress, buffer, 4).failure == MemoryReadFailure::NotReadable, "no access");
Require(reader.ReadBytes(guardAddress, buffer, 4).failure == MemoryReadFailure::GuardPage, "guard");
Require(reader.ReadBytes(regionEnd - 2, buffer, 4).failure == MemoryReadFailure::CrossRegion, "cross region");
Require(reader.ReadCString(unterminatedAddress, 8).failure == MemoryReadFailure::Unterminated, "terminator");
```

The test must restore/free every allocation in RAII cleanup. Push only the RED
test registration:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/SafeMemoryReaderTests.cpp
git commit -m "test: define safe memory reader policy"
git push origin main
```

Run the CI TDD protocol and require missing `identity/SafeMemoryReader.h`.

- [ ] **Step 2: Implement the minimal reader**

Use one `VirtualQuery` call for the requested span, checked address addition, and require:

```cpp
info.State == MEM_COMMIT
(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0
base >= reinterpret_cast<std::uintptr_t>(info.BaseAddress)
end <= regionBase + info.RegionSize
```

Accept only readable protection bases (`PAGE_READONLY`, `PAGE_READWRITE`, `PAGE_WRITECOPY`, and their executable-readable variants). Copy with `std::memcpy` inside MSVC `__try/__except`; map an access fault to `AccessFault`. `ReadCString` reads at most its caller bound and the constructor cap, and requires `\0` within that bound.

- [ ] **Step 3: Verify GREEN and push**

Commit, push, and require GREEN through the CI TDD protocol:

```bash
git add CMakeLists.txt src/identity/SafeMemoryReader.* tests/SafeMemoryReaderTests.cpp tests/TestMain.cpp
git commit -m "feat: add bounded read-only memory reader"
git push origin main
```

Then rerun that successful workflow once with `gh run rerun "$run_id"` followed
by `gh run watch "$run_id" --exit-status`; both runs must pass so page cleanup is
exercised twice on independent Windows workers.

## Task 4: Map Path Validation

**Files:**
- Create: `src/identity/MapPathValidator.h`
- Create: `src/identity/MapPathValidator.cpp`
- Create: `tests/MapPathValidatorTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MapPathResult MapPathValidator::ValidateAnsi(std::string_view raw) const`.
- Produces: pure helper `IsFinalPathBeneath(std::wstring_view root,
  std::wstring_view candidate)` used after handle-based final-path resolution.
- `MapPathResult` contains `std::optional<std::string>
  normalizedRelativePath` and `MapPathFailure failure`; success requires a
  value and `MapPathFailure::None`. The value is a backslash-separated UTF-8
  path relative to the game directory.

- [ ] **Step 1: Push RED path tests**

Create a temporary game root containing `Map\User\Stable.map`. Require
acceptance of `Map/User/Stable.map` and case-insensitive file lookup on Windows.
Require rejection of an empty string, control character, absolute/UNC/drive
path, any `..` segment, non-`Map` root, wrong extension, directory in place of
file, missing file, and lexical escape. Test `IsFinalPathBeneath` with equal
roots, a normal child, a sibling sharing only the root-name prefix, and an
outside final path so a junction/reparse escape cannot pass containment.

Use explicit cases:

```cpp
Require(validator.ValidateAnsi("Map\\User\\Stable.map").normalizedRelativePath ==
            "Map\\User\\Stable.map", "valid map");
Require(validator.ValidateAnsi("C:\\outside.map").failure == MapPathFailure::Absolute, "drive path");
Require(validator.ValidateAnsi("Map\\..\\outside.map").failure == MapPathFailure::Traversal, "traversal");
Require(validator.ValidateAnsi("Map\\User\\Stable.txt").failure == MapPathFailure::WrongExtension, "extension");
```

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/MapPathValidatorTests.cpp
git commit -m "test: define map path validation"
git push origin main
```

Run the CI TDD protocol and require missing `identity/MapPathValidator.h`.

- [ ] **Step 2: Implement exact validation order**

Decode with `MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, ...)`, reject
control characters before filesystem use, replace `/` with `\`, reject
rooted/drive/UNC input and `.`/`..` components, require the first component
case-insensitively equals `Map`, and require `.map`. Join lexically beneath the
supplied game root, open the Map root and candidate with read/share-only
`CreateFileW`, reject directories, resolve both handles with
`GetFinalPathNameByHandleW`, and require case-insensitive component-boundary
containment beneath the final Map root. Then emit game-relative UTF-8 with
backslashes. Do not call `canonical` on an untrusted path and do not enumerate
the Map tree.

- [ ] **Step 3: Verify GREEN and push**

Commit and push:

```bash
git add CMakeLists.txt src/identity/MapPathValidator.* tests/MapPathValidatorTests.cpp tests/TestMain.cpp
git commit -m "feat: validate fixed-map paths"
git push origin main
```

Run the CI TDD protocol and require GREEN with `/W4 /WX /permissive-`.

## Task 5: Fixed-Map List Attribution and Live Calibration

**Files:**
- Create: `src/identity/ListAttribution.h`
- Create: `src/identity/ListAttribution.cpp`
- Create: `tests/ListAttributionTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Create: `docs/research/phase-2-2-tab-calibration.md`

**Interfaces:**
- Produces: `FixedMapListKind { Unknown, Single, Multiplayer, Custom }`.
- Produces: `TabControlMapping { DWORD single; DWORD multiplayer; DWORD custom; }` populated only from the accepted live calibration.
- Produces: `ListAttribution::ObservePages(const PageSnapshot&)`, `ObserveClick(UINT message, DWORD elementId)`, `Reset()`, and `Current()`.

- [ ] **Step 1: Add and push RED attribution tests**

Tests use a synthetic accepted mapping `{2450, 2451, 2415}` only to exercise behavior; these values must not become production constants without live attribution. Require that `{4,22,23,25}` marks the fixed-map UI present, `WM_LBUTTONUP` on each mapped ID selects the corresponding kind, an unmapped ID yields `Unknown`, page presence alone stays `Unknown`, leaving the fixed-map set resets state, and a stale click before re-entry is not reused.

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/ListAttributionTests.cpp
git commit -m "test: define fixed-map list attribution"
git push origin main
```

Run the CI TDD protocol and require missing `identity/ListAttribution.h`.

- [ ] **Step 2: Add calibration-only event logging**

Before production mapping is admitted, `S4Listeners` must log one bounded record for each left-button release while the exact fixed-map page set is present:

```text
tab-calibration element-id={decimal element ID} active=4,22,23,25
```

Do not infer the tab in this build. Preserve the existing one-second general mouse limiter, but calibration releases must use change suppression by `(element-id, page-set)` so three deliberate actions are not lost.

- [ ] **Step 3: Build, install, and collect the calibration with user control**

Push the calibration build and require green CI. Ask the user to close both processes. Download the exact successful artifact, verify PE32 and two-file package layout, then run the guarded elevated installer only after confirming the original backup hash remains `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

Commit the calibration build before deployment:

```bash
git add CMakeLists.txt src/identity/ListAttribution.* src/runtime/S4Listeners.* \
  tests/ListAttributionTests.cpp tests/TestMain.cpp
git commit -m "feat: add fixed-map tab calibration capture"
git push origin main
```

Ask the user to launch normally and, one action at a time, click Single Player Maps, Multiplayer Maps, and Custom Maps. Record for each action the stable element ID and surrounding page set in `phase-2-2-tab-calibration.md`. Reject an ID that also fires for map rows, Back, or another tab.

- [ ] **Step 4: Admit only the observed mapping and turn calibration off**

Add `constexpr TabControlMapping kApprovedTabControls` using a fully numeric
three-value initializer in `single`, `multiplayer`, `custom` order, and cite the
three matching calibration-report rows beside the declaration. The numeric
initializers must be the exact reviewed log values; symbolic names or comments
in place of values fail review. Remove the calibration-only log path. Update the
test fixture to use `kApprovedTabControls` and keep ambiguity/reset cases.

- [ ] **Step 5: Verify GREEN and push**

Commit and push the accepted mapping:

```bash
git add src/identity/ListAttribution.* src/runtime/S4Listeners.* \
  tests/ListAttributionTests.cpp docs/research/phase-2-2-tab-calibration.md
git commit -m "feat: attribute fixed-map list tabs"
git push origin main
```

Run the CI TDD protocol and require GREEN before continuing.

## Task 6: Immutable Observation and Session State

**Files:**
- Create: `src/identity/MapIdentityObservation.h`
- Create: `src/identity/MapIdentityObservation.cpp`
- Create: `src/identity/MapIdentityState.h`
- Create: `src/identity/MapIdentityState.cpp`
- Create: `tests/MapIdentityStateTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces enums `IdentitySource`, `IdentityConfidence`, `IdentityFailure`,
  struct `IdentityReadResult`, abstract interface `IIdentitySampler`, and struct
  `MapIdentityObservation` matching the approved design.
- Produces: `ObserveMenu(FixedMapListKind, DWORD, IdentityReadResult, std::uint64_t)` and `BeginLoadedSession(IdentityReadResult, std::uint64_t)` returning an optional changed observation.
- Produces: `FormatIdentityObservation(const MapIdentityObservation&)` with bounded, pointer-free output.

- [ ] **Step 1: Push RED state-machine tests**

Require: a verified menu sample becomes `confirmed`; UI-only data remains `unknown`; `map-init` increments `sessionId`; matching normalized path produces a corroborated loaded observation; a menu/loaded conflict preserves both diagnostic values but sets automatic confidence to `unknown` and failure `MenuLoadedConflict`; a new session clears the previous loaded sample; returning to menu never emits victory/completion; and repeating an identical observation emits no log record.

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/MapIdentityStateTests.cpp
git commit -m "test: define map identity session transitions"
git push origin main
```

Run the CI TDD protocol and require missing `identity/MapIdentityState.h`.

- [ ] **Step 2: Implement immutable transition rules**

`MapIdentityObservation` contains exactly:

```cpp
FixedMapListKind listKind;
DWORD selectedElementId;
std::optional<std::string> mapRelativePath;
std::optional<std::uint32_t> mapInternalId;
std::optional<std::string> displayNameCandidate;
IdentitySource source;
IdentityConfidence confidence;
std::uint64_t sampledAtMs;
std::uint64_t sessionId;
IdentityFailure failure;
```

`IdentityReadResult` contains optional normalized relative path, optional
internal ID, and one `IdentityFailure`. `IIdentitySampler` declares a virtual
destructor and `virtual IdentityReadResult Sample() const noexcept = 0`; this is
the sole seam used by probe tests.

Compare paths case-insensitively. Format only enum names, bounded IDs, normalized game-relative path, timestamp, session, and failure; never format raw pointers or arbitrary memory.

- [ ] **Step 3: Verify GREEN and push**

Commit and push:

```bash
git add CMakeLists.txt src/identity/MapIdentityObservation.* \
  src/identity/MapIdentityState.* tests/MapIdentityStateTests.cpp tests/TestMain.cpp
git commit -m "feat: add immutable map identity sessions"
git push origin main
```

Run the CI TDD protocol and require GREEN.

## Task 7: Verified Map Object Reader

**Files:**
- Create: `src/identity/VerifiedMapIdentityReader.h`
- Create: `src/identity/VerifiedMapIdentityReader.cpp`
- Create: `tests/VerifiedMapIdentityReaderTests.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/research/phase-2-2-static-map-identity-evidence.md`

**Interfaces:**
- Consumes: `ExecutableLayout`, `SafeMemoryReader`, `MapPathValidator`, and the exact Task 1 `MapObjectLayout`.
- Produces: `IdentityReadResult VerifiedMapIdentityReader::Sample() const noexcept` by implementing `IIdentitySampler`.
- `IdentityReadResult` contains optional normalized path/internal ID plus a stable `IdentityFailure`; it exposes no pointer.

- [ ] **Step 1: Push RED synthetic-chain tests**

Build a synthetic readable object graph with a root pointer, the exact number of pointer steps supported by `MapObjectLayout`, a vtable field, an ANSI map path, and optional ID. Use a temporary real `.map` file. Require one accepted sample and rejection of null root, unreadable nested pointer, wrong vtable, vtable outside `.rdata`, unterminated string, invalid path, missing file, unstable repeated samples, and offset/address overflow.

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/VerifiedMapIdentityReaderTests.cpp
git commit -m "test: define verified map object reads"
git push origin main
```

Run the CI TDD protocol and require missing
`identity/VerifiedMapIdentityReader.h`.

- [ ] **Step 2: Implement the exact chain without scanning**

Resolve only `rootPointer` and `cMapFileVtable` from `ExecutableLayout`. Read the root pointer, then each of `dereferenceCount` offsets in order with checked addition and a new `ReadPointer`. Require the final object's first pointer to equal the resolved CMapFile vtable when Task 1 identifies the final object as `CMapFile`. Read only `pathOffset` using the accepted `PathStorage`; read the optional ID only at `internalIdOffset`. Validate the path, sample the complete identity twice on the same trigger, and accept only identical results.

No loop may enumerate memory regions or search for `.map`; the only loop permitted here is the fixed, maximum-four evidence offset loop.

- [ ] **Step 3: Transcribe and cross-check production layout constants**

Add a compile-time `ApprovedMapObjectLayout()` whose numeric values exactly match the accepted evidence report. Add `static_assert` checks for the maximum chain length and x86 pointer size in the ASI target. Cross-reference every production initializer with its evidence table row and verify all computed RVAs against `objdump -h`.

- [ ] **Step 4: Verify GREEN and push**

Commit and push:

```bash
git add CMakeLists.txt src/identity/VerifiedMapIdentityReader.* \
  tests/VerifiedMapIdentityReaderTests.cpp tests/TestMain.cpp \
  docs/research/phase-2-2-static-map-identity-evidence.md
git commit -m "feat: read verified fixed-map identity"
git push origin main
```

Run the CI TDD protocol, rerun the successful workflow once, and require both
runs GREEN. Do not deploy if any production initializer lacks an
instruction-level evidence row.

## Task 8: Probe and S4ModApi Callback Integration

**Files:**
- Create: `src/identity/MapIdentityProbe.h`
- Create: `src/identity/MapIdentityProbe.cpp`
- Create: `tests/MapIdentityProbeTests.cpp`
- Modify: `src/runtime/S4Listeners.h`
- Modify: `src/runtime/S4Listeners.cpp`
- Modify: `src/runtime/DiagnosticRuntime.h`
- Modify: `src/runtime/DiagnosticRuntime.cpp`
- Modify: `config/CampaignCompletionDebug.ini`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MapIdentityProbe::ObservePages`, `ObserveMouse`, `ObserveMapInit`, and `Disable`.
- Changes: `S4Listeners::Start(S4API, Logger&, MapIdentityProbe&)`.
- Changes: runtime bootstrap header to version `0.2.2` and `hook-mode=public-only memory-mode=verified-read-only`.

- [ ] **Step 1: Push RED probe orchestration tests**

Inject a deterministic `IIdentitySampler` test double. Require no sample on unrelated pages, one menu sample after a mapped `WM_LBUTTONUP`, no repeated sample for unchanged UI action, a new loaded sample on every `map-init`, conflict downgrade, explicit diagnostic resample only after validation-state change, and permanent `unknown` after `Disable`. Verify produced log strings contain no `0x` pointer values, `victory`, `completion-json`, or absolute user path.

Push RED:

```bash
git add CMakeLists.txt tests/TestMain.cpp tests/MapIdentityProbeTests.cpp
git commit -m "test: define read-only map identity probe"
git push origin main
```

Run the CI TDD protocol and require missing `identity/MapIdentityProbe.h`.

- [ ] **Step 2: Implement event-triggered sampling**

`ObservePages` updates attribution and samples only when stable fixed-map presence changes. `ObserveMouse` passes only a left-button release with a non-null element ID. `ObserveMapInit` always starts a new session and samples the loaded identity. Cache the last emitted observation/failure so frames and identical callbacks produce no repeated lines.

Wrap the entire probe call at the S4 callback boundary with `__try/__except`; on fault call `Disable(IdentityFailure::AccessFault)` and emit one error. Do not catch and resume dereferencing inside the failed chain.

- [ ] **Step 3: Construct only after executable compatibility**

In `DiagnosticRuntime::Start`, after the existing exact version/hash check, build `ExecutableLayout` from the live executable module, construct the validator from `gameDirectory`, then the verified reader and probe. If construction fails, retain public listener diagnostics and controlled stop but pass a disabled probe that reports one stable reason.

Set the INI policy exactly:

```ini
[Diagnostic]
Version=0.2.2
LogLevel=Info
ModuleInventory=1
PublicListeners=1
VerifiedMemoryReads=1
InternalHooks=0
MemoryWrites=0
InternalFunctionCalls=0
CompletionDetection=0
CompletionStorage=0
CompletionMarkers=0
ControlledStopFile=CampaignCompletionDebug.stop
```

- [ ] **Step 4: Verify GREEN, safety policy, and package**

Run:

```bash
rg -n "WriteProcessMemory|VirtualProtect|CreateRemoteThread|MinHook|MH_CreateHook|CallPatch|JmpPatch|NopPatch" src
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test_settlers_united_artifact.ps1
git add CMakeLists.txt config/CampaignCompletionDebug.ini src/identity \
  src/runtime/DiagnosticRuntime.* src/runtime/S4Listeners.* \
  tests/MapIdentityProbeTests.cpp tests/TestMain.cpp
git commit -m "feat: integrate read-only map identity probe"
git push origin main
```

Expected: the forbidden-operation scan has no match in `src`; the local archive
test passes. Run the CI TDD protocol and require PE machine `0x014c`, exact
two-file ZIP layout, complete CTest success, and green artifact upload.

## Task 9: Live Identity Acceptance and Phase Report

**Files:**
- Create: `docs/research/phase-2-2-read-only-map-identity-report.md`
- Modify only if evidence requires correction: `docs/research/phase-2-2-static-map-identity-evidence.md`

**Interfaces:**
- Consumes: exact successful CI run/artifact, guarded SU archive tools, user-driven game actions, plugin log, protected hashes, and backup metadata.
- Produces: `GO`, `PARTIAL/NO-GO`, or `NO-GO` with a reproducible evidence table.

- [ ] **Step 1: Freeze and verify the CI artifact**

Record commit, workflow run ID, artifact ID/digest, outer ZIP SHA-256, inner package SHA-256, ASI SHA-256, INI SHA-256, PE machine, and exact archive entries. Require the artifact commit to equal `origin/main` and require all GitHub checks green.

Before deployment, count regular `*.map` files recursively beneath the installed
game `Map` root without changing them and require the approved baseline count
`373`; record the command and result in the report.

```bash
powershell.exe -NoProfile -Command \
  "@(Get-ChildItem -LiteralPath 'F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\Map' -Filter '*.map' -File -Recurse).Count"
```

Expected: `373`.

- [ ] **Step 2: Guarded deployment with backup preflight**

Ask the user to close the game and Settlers United. Confirm neither process is present. Verify:

```text
original backup SHA-256 = 807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265
installed archive SHA-256 = the current metadata patchedSha256
restore readiness = true
```

Use only `tools/install_settlers_united_artifact.ps1` in an elevated user-run PowerShell to replace `Plugins/CampaignCompletionDebug.asi` inside the authorized archive. Re-open the archive read-only and record the embedded ASI hash.

- [ ] **Step 3: Run the three-list and fixed-map scenario**

Ask the user to launch normally through Settlers United. In order:

1. enter Single Player Maps, select a fixed map, and retain the menu identity;
2. enter Multiplayer Maps and select a different entry;
3. enter Custom Maps and select a different entry;
4. return to Single Player Maps, select the test fixed map, and launch it;
5. remain in the initialized map long enough for the post-`map-init` stable sample;
6. exit normally without winning.

Require each tab to log the calibrated `list_kind`, and require the selected Single Player map's normalized relative path or internal ID to agree before/after init. A read failure or disagreement stays explicit `unknown`; do not retry with a scan.

- [ ] **Step 4: Prove controlled stop and integrity**

With the process still responsive, create only `CampaignCompletion\CampaignCompletionDebug.stop`. Require `registered=75 removed=75 failures=0`, stop-file consumption, no later UI/mouse/map-init/identity callback lines after deliberate navigation, and process responsiveness. Recompute the 12 protected hashes from Phase 2.1 plus installed archive, original backup, embedded ASI, and restore-readiness checks.

- [ ] **Step 5: Write, review, commit, and push the report**

The report must include:

- static root/chain evidence references without raw pointer dumps;
- approved tab ID mapping and calibration actions;
- menu and loaded observations, sources, confidence, session IDs, and comparison;
- every failure reason encountered;
- CI/package/deployment hashes;
- listener stop and post-stop silence proof;
- protected-file and backup results;
- one decision using the approved definitions.

Decision rules:

```text
GO: all three tabs attributed, one fixed-map identity agrees menu/post-init,
    no hook/write/call/scan, stop/integrity checks pass.
PARTIAL/NO-GO: tabs attributed but stable identity is absent or conflicts.
NO-GO: safety boundary, executable identity, listener stop, or integrity fails.
```

Run `git diff --check`, verify no backup/binary/log entered the index, then:

```bash
git add docs/research/phase-2-2-read-only-map-identity-report.md \
  docs/research/phase-2-2-static-map-identity-evidence.md
git diff --cached --name-only
git commit -m "docs: report phase 2.2 map identity results"
git push origin main
```

Expected staged paths: only the report and, when changed, the static evidence
document; no binary, log, backup, or generated backup metadata.

## Final Verification Checklist

- [ ] `git status --short` is empty and `HEAD == origin/main`.
- [ ] PowerShell archive tests pass.
- [ ] Win32 configure/build and complete CTest pass.
- [ ] Forbidden-operation source scan is empty.
- [ ] PE machine is `0x014c`; package contains exactly the ASI and INI.
- [ ] Static evidence maps every production RVA/offset to reviewed instructions and PE sections.
- [ ] The three list kinds require fixed pages plus calibrated click IDs.
- [ ] At least one menu/post-init fixed-map identity matches, or the report says `PARTIAL/NO-GO` without fallback.
- [ ] No victory, completion storage, completion JSON, or marker behavior exists.
- [ ] Controlled stop removes all 75 listeners with zero failures and post-stop silence.
- [ ] All protected hashes, original backup hash, embedded ASI hash, and restore readiness pass.
