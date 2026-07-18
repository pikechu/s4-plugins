# Phase 2.1 Diagnostic Hardening Design

Date: 2026-07-13  
Status: Approved in conversation; pending written-spec review  
Target: The Settlers IV History Edition through Settlers United

## Goal

Close the three gaps identified by the Phase 2 diagnostic report:

1. make `CampaignCompletionDebug.asi` survive Settlers United's pre-launch
   plugin synchronization;
2. attribute public UI-frame callbacks to their exact registered pages instead
   of masking nested pages with lower-valued parent pages;
3. exercise controlled listener removal in the live game and prove that the
   plugin becomes silent without destabilizing the process.

Phase 2.1 remains diagnostic-only. It does not detect victory, persist
completion state, create completion JSON, render completion markers, or install
an internal game hook.

## Authorization and Safety Boundary

The game installation remains read-only except for project-owned
`CampaignCompletion*.asi` files and the `CampaignCompletion` directory.

The user additionally authorizes Phase 2.1 to modify exactly this Settlers
United file after a verified backup is created:

`C:\Program Files\Settlers United\resources\bin\s4_artifacts\Plugin_SU.zip`

During atomic replacement, the integration script may briefly create the
sibling file `Plugin_SU.zip.campaigncompletion.tmp` and must remove it on both
success and failure. No other Settlers United file may be created, modified, or
deleted.

The original `Plugin_SU.zip` backup and its metadata live under
`research/backups/settlers-united/` in the repository workspace. The proprietary
binary backup and generated metadata are not committed to Git.

## Architecture

### Settlers United artifact integration

A PowerShell module owns archive inspection, backup, patch, verification, and
restore. A thin install script supplies the approved paths and the new ASI.

Installation requires both `S4_Main.exe` and the Settlers United application to
be stopped. The script:

1. reads and hashes the source archive;
2. creates the backup if absent, or validates the existing backup and metadata;
3. extracts/rebuilds a workspace copy of the archive;
4. adds or replaces only `Plugins/CampaignCompletionDebug.asi`;
5. compares the uncompressed SHA-256 of every pre-existing entry with the
   source archive;
6. verifies the embedded ASI hash against the requested ASI;
7. validates that the rebuilt file is a readable ZIP;
8. copies the verified rebuilt archive to
   `Plugin_SU.zip.campaigncompletion.tmp`;
9. replaces `Plugin_SU.zip` only after all checks pass;
10. records original and patched archive hashes, sizes, timestamps, and embedded
    ASI hash in backup metadata;
11. removes the temporary sibling in a `finally` path.

An install failure before replacement leaves the installed archive untouched.
An install failure during replacement must restore the verified backup before
returning failure.

The restore script operates only when:

- the backup hash matches recorded metadata; and
- the installed archive hash matches the recorded patched hash.

If Settlers United updated the archive, restore refuses to overwrite it and
reports the mismatch. Re-running installation against an updated archive
requires an explicit new backup generation step after the previous backup is
preserved by the user.

### Exact UI-page attribution

The shared UI-frame callback is replaced with one compile-time thunk per
`S4_GUI_ENUM` value from `1` through `S4_GUI_ENUM_MAXVALUE - 1`. Every thunk
passes its registered page value into the common observer.

A fixed-size page observation window records every page thunk seen during the
one-second diagnostic interval. At flush time it produces:

- `active`: all observed page values, sorted and unique;
- `primary`: the greatest active enum value, which corresponds to the more
  specific child page in the current S4ModApi enum organization.

For example, observing parent page `4` and nested map-list page `22` produces:

`ui-pages active=4,22 primary=22`

The complete `active` list is authoritative diagnostic evidence; `primary` is a
convenience for associating mouse and GUI-element summaries. The observation
window resets only after a summary is emitted.

No callback calls `IsCurrentlyOnScreen`, enumerates modules, hashes files, or
performs unbounded allocation.

### Controlled stop and callback quiescence

The bootstrap thread remains alive as a lightweight control thread after
successful initialization. It polls only this authorized request file:

`CampaignCompletion/CampaignCompletionDebug.stop`

The request is not created by the plugin. Test tooling or the user creates it
while the game is running. The request is consumed once; the plugin removes it
after detection.

Listener callbacks enter through a lifecycle guard. Controlled stop:

1. prevents new callbacks from entering;
2. waits for callbacks already in progress to leave the guarded region;
3. removes stored listener handles in reverse registration order;
4. records `registered`, `removed`, and `failures` counts;
5. releases the S4ModApi interface;
6. logs `diagnostic runtime stopped` and closes the logger;
7. leaves the ASI loaded but inert until process exit.

The removal implementation is separated into a pure, testable reverse-order
policy and the S4ModApi adapter. No detach cleanup runs under loader lock.

The bootstrap header includes PID so that Settlers United's short-lived startup
process can be distinguished from the surviving game process.

## Error Handling

- Archive integration fails closed on missing paths, running processes, corrupt
  ZIP data, entry hash mismatch, ASI hash mismatch, backup mismatch, or an
  unrecognized installed archive state.
- Archive scripts never accept an arbitrary replacement target; the destination
  file name must be exactly `Plugin_SU.zip` beneath an `s4_artifacts` directory.
- Listener registration failure removes every successfully registered handle in
  reverse order before releasing the API.
- A stop request before initialization completes is handled after successful
  initialization and results in an immediate controlled stop.
- Listener removal failures are counted and logged without retry loops. Any
  nonzero failure count fails Phase 2.1 runtime acceptance.
- After controlled stop, callbacks return without touching logger or API state.

## Automated Tests

Tests follow red-green TDD and run in GitHub Actions on Windows.

### Archive tests

A temporary sample archive proves:

- first installation creates a byte-identical original backup;
- patching preserves the uncompressed content hash of every original entry;
- the requested ASI appears at exactly
  `Plugins/CampaignCompletionDebug.asi`;
- re-patching replaces only that entry;
- restoring reproduces the exact original archive hash;
- restore refuses after an external archive mutation;
- a failed verification leaves the installed sample archive unchanged;
- temporary sibling cleanup occurs on success and failure.

### Native tests

- A page window observing `4`, `22`, and duplicate `22` yields
  `active={4,22}` and `primary=22`, then resets to empty.
- The reverse-removal policy receives handles `{10,20,30}` in the order
  `{30,20,10}`.
- Mixed removal results report exact registered, removed, and failure counts.
- The stop-request helper returns false without a file, true once when the file
  appears, consumes it, and returns false again.
- Existing logger, module-inventory, compatibility, and rate-limiter tests remain
  green.

The workflow continues to require MSVC Win32, PE machine `0x014c`, tests, and the
existing two-file deployment ZIP layout.

## Live Acceptance Test

1. Close `S4_Main.exe` and the Settlers United application.
2. Create and verify the repository backup of `Plugin_SU.zip`.
3. Patch the archive with the new CI-built diagnostic ASI.
4. Start Settlers United normally without a watcher.
5. Verify that the ASI remains in `Plugins`, loads into the formal game process,
   and logs the PID, exact executable identity, S4ModApi identity, and public-only
   mode.
6. Visit Single Player Maps, Multiplayer Maps, and Custom Maps. The log must
   include their specific page thunks and must retain any simultaneously active
   parent pages in `active`.
7. From a stable menu, create `CampaignCompletionDebug.stop`.
8. Require one stop summary with `failures=0` and `removed=registered`.
9. Continue menu interaction for at least five seconds. Require no UI-frame,
   mouse, GUI-element, or map-init record after the stop summary and require the
   game process to remain responsive.
10. Run `sha256sum -c research/evidence/manifest.sha256` and require all 12
    protected inputs to report `OK`.
11. Record original/patched Settlers United archive hashes and restore readiness
    in the Phase 2.1 report.

The patched archive remains installed after successful acceptance so subsequent
diagnostic launches remain persistent. The verified backup remains available
for explicit restoration.

## Completion Criteria

Phase 2.1 is complete only when:

- the CI artifact is green;
- normal Settlers United launch loads the diagnostic ASI without a watcher;
- visited nested map-list pages are attributed by their own thunks;
- controlled stop reports full successful reverse removal;
- the post-stop quiet period and process responsiveness checks pass;
- all protected-input hashes remain unchanged;
- backup and restore readiness are documented.

Only after all criteria pass may the project reconsider a direct internal probe.
