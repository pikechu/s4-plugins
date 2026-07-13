# Campaign Completion Phase 1 Investigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a reproducible, evidence-backed investigation report for S4ModApi 2.3.2, `S4_Main.exe` 2.50.1516.0, `S4_MainR.pdb`, Settlers United, and the installed ASI loading chain without guessing offsets or installing a diagnostic plugin.

**Architecture:** Store repeatable read-only collection scripts under `research/scripts`, normalized command outputs under `research/evidence`, and conclusions with confidence labels under `docs/research`. Separate raw evidence from interpretation so every proposed probe or Hook point can be traced to a source symbol, public API declaration, binary property, or observed process/module state.

**Tech Stack:** Bash, PowerShell 7/Windows PowerShell, GNU binutils (`objdump`, `strings`), Git, GitHub source at `nyfrk/S4ModApi`, PE/CodeView tooling selected during the tooling audit, Windows process/module inspection.

## Global Constraints

- Target game: The Settlers IV History Edition, S4 3.15, `S4_Main.exe` file version `2.50.1516.0`.
- Target architecture: Win32/x86 only; never produce or analyze an x64 plugin as a compatible deliverable.
- Local S4ModApi: `S4ModApi.dll` file version `2.3.2.0`.
- Game directory is read-only except future test deployment may add, replace, or remove only `CampaignCompletion*.asi` and the `CampaignCompletion` directory.
- Settlers United installation is read-only.
- Launching the game may produce normal game or Settlers United logs, configuration, and cache files.
- Phase 1 installs no Hook, injects no DLL, writes no completion data, and deploys no ASI.
- Do not infer APIs such as `OnVictory`, `GetCurrentMapName`, `IsRandomMap`, or `IsMultiplayer`; record only declarations and behavior supported by evidence.
- Every conclusion must be labeled `Confirmed`, `Candidate`, or `Unknown` and cite an evidence file plus line or symbol name.
- Do not publish raw absolute addresses as stable integration points; record module-relative RVA, symbol identity, signature context, and version constraints.

---

## Planned File Structure

- `research/README.md`: explains evidence provenance, regeneration commands, and privacy rules.
- `research/scripts/common.sh`: canonical read-only paths and output helpers.
- `research/scripts/collect_pe_metadata.sh`: gathers hashes, PE architecture, imports, exports, and version resources.
- `research/scripts/collect_s4modapi.sh`: pins and inventories the public S4ModApi source.
- `research/scripts/collect_pdb_symbols.ps1`: emits normalized PDB symbol data using the selected CodeView/DIA tool.
- `research/scripts/collect_loader_state.ps1`: launches Settlers United when requested and records process/module state without injection.
- `research/evidence/.gitkeep`: retains the evidence directory while generated machine-specific files remain ignored.
- `research/evidence/manifest.sha256`: generated hash manifest for all analyzed installed binaries.
- `research/evidence/pe-metadata.txt`: normalized PE, version, import, and export evidence.
- `research/evidence/s4modapi-source.txt`: source commit and relevant API declarations.
- `research/evidence/pdb-symbols.txt`: normalized candidate symbols, section/RVA data, and undecorated names.
- `research/evidence/loader-state.txt`: observed game process and loaded module inventory.
- `docs/research/phase-1-investigation-report.md`: conclusions, candidate probes, risks, and phase 2 acceptance criteria.
- `.gitignore`: ignores transient source clones and machine-specific raw dumps while retaining normalized evidence.

## Task 1: Establish Reproducible Evidence Boundaries

**Files:**
- Create: `research/README.md`
- Create: `research/scripts/common.sh`
- Create: `research/evidence/.gitkeep`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: Approved design constraints in `docs/superpowers/specs/2026-07-13-campaign-completion-design.md`.
- Produces: `GAME_DIR`, `SU_DIR`, `EVIDENCE_DIR`, `require_readable_file()`, and `write_evidence_header()` for later collection scripts.

- [ ] **Step 1: Add a failing shell contract check**

Run before creating `common.sh`:

```bash
bash -c 'source research/scripts/common.sh; test "$GAME_DIR" = "/mnt/f/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/thesettlers4"; test "$SU_DIR" = "/mnt/c/Program Files/Settlers United"'
```

Expected: FAIL because `research/scripts/common.sh` does not exist.

- [ ] **Step 2: Create the shared path and safety helper**

Create `research/scripts/common.sh` with:

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly WORKSPACE_DIR="/mnt/f/claude projects/thesettler4plugin"
readonly GAME_DIR="/mnt/f/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/thesettlers4"
readonly SU_DIR="/mnt/c/Program Files/Settlers United"
readonly EVIDENCE_DIR="$WORKSPACE_DIR/research/evidence"

require_readable_file() {
    local path="$1"
    [[ -f "$path" && -r "$path" ]] || {
        printf 'Required readable file missing: %s\n' "$path" >&2
        return 1
    }
}

write_evidence_header() {
    local title="$1"
    printf '# %s\n' "$title"
    printf 'generated_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'workspace_commit=%s\n' "$(git -C "$WORKSPACE_DIR" rev-parse HEAD)"
}
```

- [ ] **Step 3: Document provenance and prohibited writes**

Create `research/README.md` stating that installed game and Settlers United files are read-only inputs, generated evidence is written only beneath `research/evidence`, process launches may cause application-owned writes, and phase 1 never deploys an ASI. Include exact regeneration commands for each later script.

- [ ] **Step 4: Add evidence ignore rules**

Create or modify `.gitignore` with:

```gitignore
research/vendor/
research/evidence/raw/
research/evidence/*.etl
research/evidence/*.dmp
research/evidence/*.pdb.json
```

Create the empty `research/evidence/.gitkeep` file.

- [ ] **Step 5: Verify the contract and repository cleanliness**

Run:

```bash
bash -c 'source research/scripts/common.sh; require_readable_file "$GAME_DIR/S4_Main.exe"; require_readable_file "$GAME_DIR/S4_MainR.pdb"; require_readable_file "$GAME_DIR/S4ModApi.dll"; test -d "$SU_DIR"'
git diff --check
```

Expected: both commands exit 0 without output.

- [ ] **Step 6: Commit the evidence framework**

```bash
git add .gitignore research/README.md research/scripts/common.sh research/evidence/.gitkeep
git commit -m "chore: establish read-only research framework"
```

## Task 2: Inventory Installed PE Files and Loading Dependencies

**Files:**
- Create: `research/scripts/collect_pe_metadata.sh`
- Create: `research/evidence/manifest.sha256`
- Create: `research/evidence/pe-metadata.txt`

**Interfaces:**
- Consumes: `GAME_DIR`, `SU_DIR`, `EVIDENCE_DIR`, `require_readable_file()`, and `write_evidence_header()` from `common.sh`.
- Produces: stable SHA-256 identities and normalized PE facts consumed by the final report.

- [ ] **Step 1: Write the failing output contract**

Run before creating the collector:

```bash
bash research/scripts/collect_pe_metadata.sh
test -s research/evidence/manifest.sha256
rg -q 'S4_Main.exe.*pei-i386' research/evidence/pe-metadata.txt
rg -q 'S4ModApi.dll.*2.3.2.0' research/evidence/pe-metadata.txt
rg -q 'SettlersUnited.asi.*S4ModApi.dll' research/evidence/pe-metadata.txt
```

Expected: FAIL because the collector does not exist.

- [ ] **Step 2: Implement deterministic PE collection**

Create `research/scripts/collect_pe_metadata.sh`. It must:

1. Source `common.sh`.
2. Enumerate only these read-only inputs:
   - `S4_Main.exe`
   - `S4_MainR.pdb`
   - `S4ModApi.dll`
   - `ddraw.dll`
   - `Plugins/SettlersUnited.asi`
   - `Plugins/SettlersUnitedLogger.asi`
   - `Plugins/S4HD-Patch.asi`
   - `Plugins/S4WarriorsLib.asi`
   - `Plugins/ExtraZoom.asi`
   - Settlers United `resources/bin/s4u.exe`
   - Settlers United `resources/bin/s4.dll`
   - Settlers United `resources/bin/settlers-united.dll`
3. Generate hashes using `sha256sum` in lexical path order.
4. Record `file` and `objdump -f` architecture output.
5. Record PE imports and exports using `objdump -p`.
6. Query Windows version resources with this exact PowerShell expression:

```powershell
$v = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($path)
"FileVersion=$($v.FileVersion);ProductVersion=$($v.ProductVersion);Company=$($v.CompanyName)"
```

7. Normalize installed prefixes to `${GAME_DIR}` and `${SU_DIR}` before saving evidence.
8. Never redirect output into either installed directory.

- [ ] **Step 3: Run collection and validate required facts**

```bash
bash research/scripts/collect_pe_metadata.sh
test "$(wc -l < research/evidence/manifest.sha256)" -eq 12
rg 'S4_Main.exe.*pei-i386' research/evidence/pe-metadata.txt
rg 'S4_Main.exe.*FileVersion=2, 50, 1516, 0' research/evidence/pe-metadata.txt
rg 'S4ModApi.dll.*FileVersion=2.3.2.0' research/evidence/pe-metadata.txt
rg 'SettlersUnited.asi.*DLL Name: S4ModApi.dll' research/evidence/pe-metadata.txt
```

Expected: manifest count is 12 and every `rg` command prints one or more matching lines.

- [ ] **Step 4: Verify no protected input changed**

Run the collector a second time, then compare the manifest with its first-run copy stored in `/tmp/campaign-completion-manifest.first`:

```bash
cp research/evidence/manifest.sha256 /tmp/campaign-completion-manifest.first
bash research/scripts/collect_pe_metadata.sh
cmp /tmp/campaign-completion-manifest.first research/evidence/manifest.sha256
```

Expected: `cmp` exits 0.

- [ ] **Step 5: Commit the PE inventory**

```bash
git add research/scripts/collect_pe_metadata.sh research/evidence/manifest.sha256 research/evidence/pe-metadata.txt
git commit -m "research: inventory game and loader binaries"
```

## Task 3: Pin and Audit the Public S4ModApi Interface

**Files:**
- Create: `research/scripts/collect_s4modapi.sh`
- Create: `research/evidence/s4modapi-source.txt`

**Interfaces:**
- Consumes: public repository `https://github.com/nyfrk/S4ModApi.git` and local DLL hash/version evidence.
- Produces: pinned source commit plus exact declarations for page, UI, map-init, input, scripting, and absent convenience APIs.

- [ ] **Step 1: Write the failing source-evidence assertions**

```bash
bash research/scripts/collect_s4modapi.sh
rg -q '^commit=[0-9a-f]{40}$' research/evidence/s4modapi-source.txt
rg -q 'AddMapInitListener' research/evidence/s4modapi-source.txt
rg -q 'AddGuiElementBltListener' research/evidence/s4modapi-source.txt
rg -q 'S4_SCREEN_SINGLEPLAYER_MAPSELECT_RANDOM' research/evidence/s4modapi-source.txt
rg -q 'absence_check:OnVictory=not_declared' research/evidence/s4modapi-source.txt
```

Expected: FAIL before the collector exists.

- [ ] **Step 2: Implement source pinning and declaration extraction**

Create `research/scripts/collect_s4modapi.sh` that:

1. Clones or updates the repository only under `research/vendor/S4ModApi`.
2. Resolves and records the full `HEAD` commit.
3. Records the latest release tag pointing at or preceding that commit.
4. Extracts declarations and nearby documentation for:
   - `S4CreateInterface` and `S4ApiCreate`
   - `AddFrameListener`
   - `AddUIFrameListener`
   - `AddMapInitListener`
   - `AddMouseListener`
   - `AddTickListener`
   - `AddGuiBltListener`
   - `AddGuiElementBltListener`
   - `AddGuiClearListener`
   - `GetHoveringUiElement`
   - `IsCurrentlyOnScreen`
   - `CreateCustomUiElement`
   - `GetLocalPlayer`
   - `HasPlayerLost`
   - `GameDefaultGameEndCheck`
5. Extracts the complete `S4_GUI_ENUM`, `S4UiElement`, and `S4GuiElementBltParams` definitions.
6. Searches declarations for `OnVictory`, `OnGameEnd`, `GetCurrentMapName`, `IsRandomMap`, `IsMultiplayer`, and `GetCampaignMenuButtons`, recording `not_declared` only when the complete public header has no match.
7. Records relevant implementation file names without treating private implementation details as ABI.

- [ ] **Step 3: Run and inspect the API evidence**

```bash
bash research/scripts/collect_s4modapi.sh
rg '^commit=[0-9a-f]{40}$' research/evidence/s4modapi-source.txt
rg 'AddUIFrameListener|AddMapInitListener|AddGuiElementBltListener|GetHoveringUiElement' research/evidence/s4modapi-source.txt
rg 'absence_check:(OnVictory|OnGameEnd|GetCurrentMapName|IsRandomMap|IsMultiplayer|GetCampaignMenuButtons)=not_declared' research/evidence/s4modapi-source.txt
```

Expected: one pinned commit, all required supported declarations, and six explicit absence results.

- [ ] **Step 4: Cross-check source ABI against the installed DLL**

```bash
objdump -p "$GAME_DIR/S4ModApi.dll" | rg 'S4CreateInterface|_S4CreateInterface@8'
rg 'S4CreateInterface' research/evidence/s4modapi-source.txt
```

Expected: both installed DLL exports and public source declarations are present. The report must still note that source `HEAD` may be newer than installed 2.3.2 and only ABI-visible methods available through the 2.x interface may be used.

- [ ] **Step 5: Commit the API audit**

```bash
git add research/scripts/collect_s4modapi.sh research/evidence/s4modapi-source.txt
git commit -m "research: audit public S4ModApi capabilities"
```

## Task 4: Select and Validate a PDB Inspection Tool

**Files:**
- Create: `docs/research/pdb-tooling-decision.md`
- Create: `research/scripts/collect_pdb_symbols.ps1`
- Create: `research/evidence/pdb-symbols.txt`

**Interfaces:**
- Consumes: `S4_MainR.pdb`, `S4_Main.exe`, their hashes, and available CodeView/DIA tools.
- Produces: normalized function/data/type candidates with undecorated names and module-relative section/RVA information.

- [ ] **Step 1: Audit available tooling without installing system-wide software**

Run:

```bash
find '/mnt/c/Program Files' '/mnt/c/Program Files (x86)' -type f \
  \( -iname 'llvm-pdbutil.exe' -o -iname 'cvdump.exe' -o -iname 'dia2dump.exe' -o -iname 'msdia*.dll' \) \
  2>/dev/null | sort
```

Expected: a deterministic list, which may be empty. Record it in the decision document.

- [ ] **Step 2: Choose the least invasive viable tool**

Use this decision order:

1. Existing `llvm-pdbutil.exe` with `dump -symbols -types -section-contribs`.
2. Existing DIA SDK sample/tool with `msdia*.dll` already installed.
3. A pinned portable LLVM archive downloaded and extracted under `research/vendor/llvm-pdbutil`, with URL and SHA-256 recorded.

The decision document must reject plain `strings` as sufficient because it does not reliably provide symbol kind, type, section, or RVA.

- [ ] **Step 3: Write a failing normalized-symbol contract**

Before implementing the collector, run:

```powershell
& research/scripts/collect_pdb_symbols.ps1
$text = Get-Content research/evidence/pdb-symbols.txt -Raw
if ($text -notmatch 'CStateVictoryScreen') { throw 'victory symbol missing' }
if ($text -notmatch 'CStateLobbyMultiplayerType') { throw 'multiplayer symbol missing' }
if ($text -notmatch 'CStateMDRandomMapParameters') { throw 'random-map symbol missing' }
if ($text -notmatch 'CMapFile') { throw 'map-file symbol missing' }
```

Expected: FAIL because the collector does not exist.

- [ ] **Step 4: Implement PDB collection**

Create `collect_pdb_symbols.ps1` with parameters defaulting to the approved PDB, EXE, tool, and evidence paths. It must:

- fail if the PDB or EXE hash differs from `manifest.sha256`;
- invoke the selected tool in read-only mode;
- retain only symbols matching this reviewed category expression:

```regex
(Victory|Defeat|GameEnd|Campaign|Mission|MapFile|RandomMap|Multiplayer|Lobby|MainMenu|AfterGame|Player.*(Won|Lost)|State.*Screen)
```

- preserve decorated name, undecorated name, symbol kind, section, offset/RVA, and type reference when the tool supplies them;
- sort by undecorated name and then RVA;
- write the selected tool version and PDB/EXE SHA-256 at the top.

- [ ] **Step 5: Generate and validate candidate symbols**

Run the PowerShell contract from Step 3 again, followed by:

```bash
rg 'CStateVictoryScreen|CStateLobbyMultiplayerType|CStateMDRandomMapParameters|CMapFile|CRandomMaps' research/evidence/pdb-symbols.txt
rg 'tool_version=|pdb_sha256=|exe_sha256=' research/evidence/pdb-symbols.txt
```

Expected: every named candidate and all provenance fields appear. Missing candidates remain explicitly documented as absent rather than replaced by guessed offsets.

- [ ] **Step 6: Commit the PDB tooling and evidence**

```bash
git add docs/research/pdb-tooling-decision.md research/scripts/collect_pdb_symbols.ps1 research/evidence/pdb-symbols.txt
git commit -m "research: extract versioned PDB symbol candidates"
```

## Task 5: Observe the Settlers United Loading Chain

**Files:**
- Create: `research/scripts/collect_loader_state.ps1`
- Create: `research/evidence/loader-state.txt`

**Interfaces:**
- Consumes: user authorization to launch the game and allow application-owned writes.
- Produces: process image, command line, bitness evidence where available, and loaded module list for the unmodified baseline game session.

- [ ] **Step 1: Write a failing loader-state contract**

Before creating the collector:

```powershell
& research/scripts/collect_loader_state.ps1 -ObserveOnly
$text = Get-Content research/evidence/loader-state.txt -Raw
if ($text -notmatch 'S4_Main.exe') { throw 'game process not observed' }
if ($text -notmatch 'S4ModApi.dll') { throw 'S4ModApi module not observed' }
if ($text -notmatch 'SettlersUnited.asi') { throw 'Settlers United plugin not observed' }
```

Expected: FAIL because the collector does not exist.

- [ ] **Step 2: Implement observation-only collection**

Create `collect_loader_state.ps1` with two modes:

- `-Launch`: starts `C:\Program Files\Settlers United\Settlers United.exe` and waits up to 120 seconds for `S4_Main.exe`.
- `-ObserveOnly`: attaches no debugger and only inspects an already running `S4_Main.exe`.

The script must use CIM for process ID, executable path, parent process ID, and command line; use `System.Diagnostics.Process.Modules` for loaded modules; sort modules by normalized lowercase path; redact the Windows account name as `${USER}`; and write only to the workspace evidence path. It must not call `WriteProcessMemory`, `CreateRemoteThread`, debugger APIs, or DLL injection tools.

- [ ] **Step 3: Run the baseline observation with user interaction**

1. Ask the user to keep the game at the main menu after launch.
2. Run:

```powershell
& research/scripts/collect_loader_state.ps1 -Launch
```

3. If Settlers United requires a manual launcher click, rerun with `-ObserveOnly` after the user confirms the game is at the main menu.

Expected: evidence lists `S4_Main.exe`, `S4ModApi.dll`, `SettlersUnited.asi`, `SettlersUnitedLogger.asi`, `S4HD-Patch.asi`, `S4WarriorsLib.asi`, and `ExtraZoom.asi`, or explicitly records any module not loaded.

- [ ] **Step 4: Verify baseline process stability**

Keep the game at the main menu for two minutes while polling process existence every ten seconds:

```powershell
1..12 | ForEach-Object {
    if (-not (Get-Process S4_Main -ErrorAction SilentlyContinue)) {
        throw 'S4_Main.exe exited during baseline observation'
    }
    Start-Sleep -Seconds 10
}
```

Expected: command exits 0. This proves only baseline launch stability, not plugin compatibility.

- [ ] **Step 5: Commit loader evidence**

```bash
git add research/scripts/collect_loader_state.ps1 research/evidence/loader-state.txt
git commit -m "research: document Settlers United loading chain"
```

## Task 6: Build the Phase 1 Investigation Report

**Files:**
- Create: `docs/research/phase-1-investigation-report.md`
- Modify: `readme.md`

**Interfaces:**
- Consumes: all normalized evidence from Tasks 2–5 and the approved design.
- Produces: the authoritative input for the separate phase 2 diagnostic-plugin plan.

- [ ] **Step 1: Create the report coverage matrix**

The report must contain one row for each required investigation question:

```markdown
| Question | Result | Confidence | Evidence | Phase 2 action |
|---|---|---|---|---|
| Can S4ModApi create UI on campaign/main-menu screens? | Public API accepts an explicit `S4_GUI_ENUM` screen and the upstream example targets `S4_SCREEN_MAINMENU`; campaign pages remain a runtime verification item. | Confirmed for main menu; Candidate for every campaign page | `s4modapi-source.txt` + declarations/example | Diagnostic plugin creates no UI, but logs page callbacks on each required screen. |
| Is custom UI restricted to in-game screens? | No public declaration imposes an in-game-only restriction; the upstream example creates main-menu UI. | Confirmed | `s4modapi-source.txt` + HelloWorld example | Verify callbacks on menu screens without drawing. |
| Is there a victory or game-end listener? | The complete public header declares no dedicated victory or game-end listener. `GameDefaultGameEndCheck` is a scripting call, not an event subscription. | Confirmed public-API absence | `s4modapi-source.txt` + absence check | Validate PDB-derived victory/game-end candidates in diagnostic-only mode. |
| Can the current map file be read through public API? | No current-map path/name getter is declared in the public interface. | Confirmed public-API absence | `s4modapi-source.txt` + absence check | Validate `CMapFile` candidates and map-init correlation. |
| Can multiplayer/random/local-winner state be read through public API? | No direct multiplayer, random-map, or local-winner getters are declared. `GetLocalPlayer` and `HasPlayerLost` provide only partial scripting information. | Confirmed public-API absence; Candidate partial signals | `s4modapi-source.txt` + declarations/absence checks | Validate separate internal fields and reject unknown values. |
| Can menu controls and draw calls be observed? | GUI element draw listeners, UI-frame listeners, hovering element data, rectangles, IDs, effects, and page enums are publicly declared. | Confirmed declarations; runtime behavior Candidate | `s4modapi-source.txt` + type declarations | Log control snapshots across the required menus before any overlay work. |
```

The final report must update confidence labels if runtime or version-specific evidence contradicts these source-level expectations.

- [ ] **Step 2: Document candidate probe points without asserting behavior**

For each PDB candidate, record:

- decorated and undecorated symbol;
- section/RVA and executable hash;
- why it may answer a requirement;
- what runtime observation would confirm it;
- failure mode if interpreted incorrectly;
- whether S4ModApi already owns a nearby Hook.

At minimum cover victory screen/state, map file, random map state, multiplayer lobby type, campaign page states, map initialization, and GUI element drawing.

- [ ] **Step 3: Define phase 2 probe acceptance criteria**

The report must require the diagnostic plugin to log, without writing completion data:

- successful x86 ASI load and deferred initialization;
- exact executable and S4ModApi versions/hashes;
- detected Settlers United modules;
- every page transition observed through S4ModApi;
- map-session start and map identity candidates;
- local-player and game-end candidates;
- multiplayer and random-map candidates;
- source and confidence for every value;
- safe disable behavior after any version/signature mismatch.

- [ ] **Step 4: Link the report from the repository README**

Add a “Development status” section to `readme.md` linking the approved design, this plan, and the phase 1 report. State that no usable plugin exists until later phases produce and validate an ASI.

- [ ] **Step 5: Run the report self-check**

```bash
rg -n 'TBD|TODO|FIXME|PLACEHOLDER' docs/research/phase-1-investigation-report.md && exit 1 || true
rg -q '## S4ModApi capabilities' docs/research/phase-1-investigation-report.md
rg -q '## PDB candidate probes' docs/research/phase-1-investigation-report.md
rg -q '## Loader observations' docs/research/phase-1-investigation-report.md
rg -q '## Phase 2 acceptance criteria' docs/research/phase-1-investigation-report.md
git diff --check
```

Expected: placeholder scan produces no matches, all required headings exist, and `git diff --check` exits 0.

- [ ] **Step 6: Commit the phase 1 report**

```bash
git add docs/research/phase-1-investigation-report.md readme.md
git commit -m "docs: report phase 1 investigation findings"
```

## Task 7: Phase 1 Completion Gate

**Files:**
- Modify only if verification exposes inaccuracies: files created by Tasks 1–6.

**Interfaces:**
- Consumes: all phase 1 commits and evidence.
- Produces: a verified phase 1 handoff with no protected-directory modifications and a clear go/no-go decision for planning phase 2.

- [ ] **Step 1: Re-run every collector**

```bash
bash research/scripts/collect_pe_metadata.sh
bash research/scripts/collect_s4modapi.sh
powershell.exe -NoProfile -File research/scripts/collect_pdb_symbols.ps1
powershell.exe -NoProfile -File research/scripts/collect_loader_state.ps1 -ObserveOnly
```

Expected: all commands exit 0 while the game is running for the final command.

- [ ] **Step 2: Verify evidence provenance and repository state**

```bash
sha256sum -c research/evidence/manifest.sha256
git diff --check
git status --short
```

Expected: every hash reports `OK`, no whitespace errors, and only intentional regenerated evidence changes appear.

- [ ] **Step 3: Confirm protected-directory scope**

Compare the pre-investigation and final manifests for the 12 analyzed inputs:

```bash
cmp /tmp/campaign-completion-manifest.first research/evidence/manifest.sha256
```

Expected: exit 0. Application-owned logs/configuration are outside this binary manifest and must be listed separately in the report if observed.

- [ ] **Step 4: Record the go/no-go decision**

Phase 2 planning is `GO` only if:

- installed binary identities are fixed by hash;
- S4ModApi ABI capabilities are documented;
- the PDB tool yields usable symbol kind and RVA data;
- the baseline loader chain is observed;
- every proposed diagnostic probe has a safe-disable rule;
- no raw offset is treated as portable across executable hashes.

Otherwise record `NO-GO`, the exact missing evidence, and the smallest safe next investigation action in the report.

- [ ] **Step 5: Commit verification corrections if needed**

```bash
git add research docs/research readme.md .gitignore
git commit -m "research: verify phase 1 evidence" || test -z "$(git status --short)"
```

Expected: either a correction commit is created or the worktree is already clean.
