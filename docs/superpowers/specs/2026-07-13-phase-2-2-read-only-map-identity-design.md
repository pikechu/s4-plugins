# Phase 2.2 Read-Only Map Identity Probe Design

Date: 2026-07-13  
Target: The Settlers IV History Edition through Settlers United  
Branch: `main`

## Goal

Phase 2.2 determines whether the current fixed-map list and a stable map identity
can be obtained without installing a new internal hook. It covers:

- Single Player Maps;
- Multiplayer Maps;
- Custom Maps;
- one fixed map launched from Single Player Maps, including a comparison between
  the menu candidate and the identity observed after map initialization.

The phase does not cover campaign missions, random-map identity, multiplayer
session state, victory detection, completion storage, or marker rendering.

## Authorization and Safety Boundary

The plugin may read the current process memory but may not write game memory,
invoke an internal game function, install a direct hook, patch machine code, or
scan the complete heap. It continues to use the public S4ModApi listeners already
verified in Phase 2.1.

The game and Settlers United installations remain read-only except for the
previously authorized project-owned `CampaignCompletion*.asi`, the
`CampaignCompletion` configuration directory, and the backed-up integration of
`CampaignCompletionDebug.asi` inside
`C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`.
No other game or Settlers United file may be changed.

If the no-hook probe cannot establish a safe object chain and stable identity,
the result is `Unknown/NO-GO`. A direct hook requires a separate design and
explicit approval; Phase 2.2 must not silently expand into one.

## Ecosystem Finding

S4ModApi and installed Settlers United plugins commonly combine public observer
interfaces with the S4ModApi `hlib` patch classes. The installed
`SettlersUnited.asi` imports `S4CreateInterface` and `Patch`, `JmpPatch`,
`CallPatch`, and `NopPatch` operations. S4ModApi itself resolves version-specific
signatures and installs shared hooks only while observers require them.

This makes a future single-point hlib observation hook ecosystem-compatible,
but it does not change the approved no-hook boundary for this phase. Existing
S4ModApi-owned hook sites must not be hooked a second time.

## Architecture

### ExecutableLayout

`ExecutableLayout` accepts only the approved executable identity:

- `S4_Main.exe` file version `2.50.1516.0`;
- SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.

It records the live module base, PE section ranges, reviewed PDB-derived RVAs,
expected section ownership, and the evidence source for every candidate offset.
Unknown hashes, integer overflow, an RVA outside the image, or a candidate in the
wrong section disables the internal probe while leaving public diagnostics and
controlled stop available.

### SafeMemoryReader

`SafeMemoryReader` is the only component allowed to dereference internal
addresses. It:

- validates every span with `VirtualQuery`;
- requires committed readable memory and rejects guard/no-access pages;
- rejects integer overflow, null pointers, cross-region reads, and spans above a
  small fixed maximum;
- copies data into plugin-owned storage before interpretation;
- reads bounded strings only and requires a terminator within the bound;
- never changes page protection and never calls `WriteProcessMemory`.

Reader failure returns a structured reason rather than throwing through an S4
callback.

### MapIdentityProbe

`MapIdentityProbe` consumes public page, mouse, GUI-element, and map-init events.
It reads only object chains admitted by `ExecutableLayout` and
`SafeMemoryReader`. It maintains a menu candidate and a separately sampled
loaded-map observation.

The probe does not run every frame. Sampling occurs only when:

- the stable page set or selected-list action changes;
- a relevant GUI selection action occurs;
- `map-init` starts a new diagnostic session;
- an explicit diagnostic resample is required after a validation state change.

## Offline Candidate Discovery

Offline analysis uses only the approved `S4_Main.exe` and `S4_MainR.pdb`. The
initial candidate set includes:

- `S4::CMapFile::sub_B11B0`;
- `CMapFile@S4` RTTI and vtable symbols;
- `SetupSingleplayerLobbyMenu`;
- `CStateLobbyMapSettings::Callbacks` and its related state functions;
- strings and cross-references associated with opening or rejecting map files.

For each candidate, the investigation must:

1. convert CodeView section:offset to an RVA using the captured PE sections;
2. disassemble the complete containing function;
3. trace relevant global and object-pointer loads;
4. document whether each offset comes from an instruction operand, an RTTI
   relation, or a verified runtime invariant;
5. reject any candidate that depends only on a guessed class layout.

The runtime layout table may contain a candidate only when PDB ownership,
instruction references, PE section ownership, and runtime invariants agree.

## Runtime Invariants

A candidate map object or identity field is accepted only when all applicable
checks pass:

- the object pointer is aligned and points to committed readable memory;
- its vtable points into the approved executable read-only image and matches the
  reviewed `CMapFile` vtable when the candidate is a `CMapFile`;
- every nested pointer passes an independent readable-span check;
- a string terminates within the fixed bound and contains no control character;
- a map path has a `.map` extension (the installed target contains 373 such
  files) and remains beneath the game `Map` root after normalization;
- the normalized path contains no absolute root, drive override, or `..` segment;
- the referenced file exists as a regular file;
- repeated samples are stable for the same UI state or diagnostic session.

Failure of any required invariant produces `Unknown` with a reason code. The
probe must not continue dereferencing the rejected chain.

## List Attribution

Phase 2.1 proved that pages `22`, `23`, and `25` are concurrently active in the
three fixed-map tabs, so the largest active page is not selected-tab identity.

`list_kind` is therefore derived from two independent public signals:

1. the active page set establishes that the fixed-map selection UI is present;
2. a calibrated mouse/GUI element action establishes which tab the user chose.

The mapping between control IDs and `single`, `multiplayer`, or `custom` is
accepted only after each control is exercised independently in a live capture.
Page number alone, display text alone, and an uncalibrated control ID all yield
`unknown`.

## Observation Model

Each sample is immutable:

```text
MapIdentityObservation
- list_kind: single | multiplayer | custom | unknown
- selected_element_id
- map_relative_path
- map_internal_id
- display_name_candidate
- source: public-ui | verified-memory | corroborated
- confidence: confirmed | corroborated | unknown
- sampled_at_ms
- session_id
- failure_reason
```

Rules:

- a verified internal path or ID may be `confirmed` only after all runtime
  invariants pass;
- UI text, list order, page values, and element IDs are corroborating signals and
  cannot alone form a stable map ID;
- relative paths use backslashes, are relative to the game directory, and are
  compared case-insensitively;
- `map-init` creates a new session and resamples the loaded identity;
- the post-init identity takes precedence over the menu candidate;
- disagreement between the menu and loaded identities is logged and both are
  downgraded to `unknown` for automatic-use purposes;
- returning to a menu is not a victory, game-end, or completion signal.

## Logging and Failure Handling

The plugin logs only identity changes, new sessions, and changes in validation
failure reason. It must not log on every UI frame.

Diagnostic records include list kind, normalized relative path or internal ID,
source, confidence, session ID, and a bounded failure reason. They do not include
raw memory dumps, arbitrary pointer contents, or unredacted user-directory
paths.

Probe failure is isolated from the existing runtime:

- an invalid layout disables only the internal reader;
- a rejected object chain leaves the menu observation at `unknown`;
- an exception or access failure cannot escape an S4 callback;
- controlled stop first closes the callback gate, then removes public listeners
  in reverse order, releases S4ModApi, and leaves the ASI inert as in Phase 2.1.

## Testing Strategy

Development follows pushed red-green TDD.

### Pure and Win32 Tests

- `ExecutableLayout`: approved identity and valid sections; wrong hash, wrong
  section, out-of-image RVA, and overflow rejection.
- `SafeMemoryReader`: readable committed page, `PAGE_NOACCESS`, guard page,
  cross-region span, null pointer, oversized span, and unterminated string.
- path normalization: valid `Map\folder\file.map`, absolute paths, `..`, control
  characters, wrong extension, path escape, and missing file.
- list attribution: calibrated element actions, all pages concurrently active,
  page-only ambiguity, and stale selection reset.
- session state: menu candidate, map-init resample, agreement, conflict downgrade,
  and new-session cleanup.

CI continues to require the PowerShell archive test, Win32 configure/build,
complete CTest, PE32 machine `0x014c`, exact two-file package layout, and artifact
upload.

### Live Acceptance

1. Close `S4_Main.exe` and Settlers United; never terminate them automatically.
2. Use the existing guarded archive integration to replace only the diagnostic
   ASI. The verified original backup must remain unchanged.
3. Launch normally through Settlers United without a watcher.
4. Visit Single Player Maps, Multiplayer Maps, and Custom Maps; exercise each
   tab independently and select different entries.
5. Launch one fixed map from Single Player Maps and compare the menu candidate
   with the post-`map-init` identity.
6. Exit normally without winning and require no victory/completion claim or
   completion JSON.
7. Consume `CampaignCompletionDebug.stop`, require complete zero-failure listener
   removal, and prove post-stop callback silence and process responsiveness.
8. Require all 12 protected hashes, installed patched-archive hash, original
   backup hash, embedded ASI hash, and restore readiness to pass.

## Completion Criteria

Phase 2.2 is `GO` only if:

- no direct hook or game-memory write is installed;
- every candidate address and object chain has documented offline evidence;
- the three list tabs are distinguished through page plus calibrated input;
- at least one fixed map has the same stable identity in the menu and after
  `map-init`;
- rejected or conflicting observations explicitly remain `unknown`;
- controlled stop, post-stop silence, game stability, protected hashes, and
  restore readiness all pass.

If tab attribution passes but stable map identity does not, the result is
`PARTIAL/NO-GO` for completion detection. No Hook or completion feature is added
as a fallback.
