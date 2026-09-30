# Music Loop Diagnostic

`MusicLoopRepair.asi` version **0.1.0** is an observation-only diagnostic draft.
It has **not been accepted in the live game** and is not a music repair release.
The reported symptom is **two pieces of music audible at the same time**.
The next step is to collect stream events around that symptom.

The plugin forwards each intercepted Miles call with its original arguments and
return value. It does not suppress, restart, pause, seek, or otherwise change
playback. It makes no additional Miles calls to query or control streams.
It observes the approved `S4_Main.exe` import slots for:

- `_AIL_open_stream@12`
- `_AIL_start_stream@4`
- `_AIL_close_stream@4`
- `_AIL_pause_stream@8`
- `_AIL_set_stream_position@8`

Compatibility admission requires executable version `2.50.1516.0` and SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`.
No hooks are installed when either check fails. Import-table reads are bounded
by the executable image. Installation rolls back installed slots on failure;
controlled stop restores a slot only if it still points to this plugin.

Audio callbacks publish fixed-size records to a preallocated queue. They do not
write files or allocate strings. A control thread records events in
`Plugins/MusicLoopRepair/MusicLoopRepair.log`. Stop closes queue admission and
waits for admitted captures before the final drain. The ASI remains loaded until
game exit; do not unload it while the game is running, including after stop.

## Independent build and package

The module uses the shared logger and executable admission implementation from
`CampaignMarker/src/diagnostics`; keep it inside this repository.
With Visual Studio C++ tools and CMake 3.24 or newer:

```powershell
cmake -S MusicDuplicatedRepair -B build-music -A Win32 -DBUILD_TESTING=ON
cmake --build build-music --config Release --parallel
cmake --build build-music --config Release --target music_diagnostic_package
```

The standalone package target creates
`build-music/MusicLoopRepair-diagnostic.zip` with exactly two files:

```text
Plugins/MusicLoopRepair.asi
Plugins/MusicLoopRepair/MusicLoopRepair.ini.example
```

Its `.zip.sha256` companion records package, binary, and example-INI hashes.
The independent `build-music-diagnostic` workflow builds Win32, runs the existing
tracker suite, verifies the PE32 machine and `MusicLoopRepairStop` export, and
publishes those two artifacts. Build success does not establish safe live-game
operation or explain the reported overlap.

The existing tracker test entry point is:

```powershell
ctest --test-dir build-music -C Release --output-on-failure --verbose -R "^music_event_tracker_tests$"
```

Those tests cover repeated starts, path replacement, window boundaries, unknown
stream handles, and starts at uptime zero. They do not exercise live IAT hooks,
concurrent capture, controlled stop, or audible overlap.

## Collecting the overlap log

1. Close the game and keep a copy of any existing music diagnostic log.
2. Extract the diagnostic package into the game directory so the ASI is at
   `Plugins/MusicLoopRepair.asi`. This is a separate diagnostic package; its
   contents do not include CampaignMarker or PileChainRepair.
3. If configuration is needed, copy `MusicLoopRepair.ini.example` to
   `MusicLoopRepair.ini` in the same directory. Without that file the default
   duplicate window is 15 seconds. Remove an old `MusicLoopRepair.stop` file
   before starting a new capture.
4. Start the game and first record the local start time. Confirm the new log
   session contains `version=0.1.0 mode=diagnostic-only`, executable admission
   `compatible`, and `runtime started ... repair-enabled=false`. An admission
   failure or missing `runtime started` line means the capture did not start.
5. Play normally. When two pieces of music overlap, note the local time to the
   nearest second, the current screen (menu or map), and the most recent action
   (for example, loading a save or switching screens). Also note whether the
   overlap stops by itself and when it stops. Capture the surrounding events;
   restarting the game can remove the useful stream history.
6. To finish the capture before game exit, create the empty file
   `Plugins/MusicLoopRepair/MusicLoopRepair.stop`. Wait for the log line
   `runtime stopped hooks-restored=true dropped-events=0`, then close the game.
   `MusicLoopRepairStop` is also an asynchronous stop-request export.
7. Retain the complete log, the timestamped observations, the INI if used, and
   the package SHA-256. A missing stop line or any dropped events limits the
   completeness of the capture. To remove the diagnostic, close the game and
   remove only `Plugins/MusicLoopRepair.asi`; retain the log for analysis.

## Reading the evidence

Every observed open, start, close, pause, and position event is logged, including
starts of different songs outside `DuplicateWindowMs`. `stream=0x...` connects a
start/pause/position/close event to the path in the stream's open event. A stream
handle can be reused after close, so keep the full sequence around each open.
Start lines repeat the known path and contain the per-handle start count.
`seq` is the capture sequence; `uptime-ms` is `GetTickCount64` at capture time.
The local timestamp prefix is the time the control thread writes the line, and
can lag capture. For concurrent producers, compare sequence numbers across
batches when reconstructing event order.

`repeat=same-stream` and `repeat=same-path` only highlight repeated start calls
inside the configured window. **A repeat warning does not prove an audible
replay or audible overlap.** Different songs may overlap without any repeat
warning. Multiple started handles without a close also do not prove audible
overlap: a naturally finished stream may remain open while already silent.
Pause and position calls provide context, not a sound-output measurement.

`event queue incomplete dropped-events=...` reports queue overflow while the
capture is running. Missing paths, `unknown-stream`, truncated long paths, or
out-of-order concurrent events need to be considered before attributing cause.
The bounded path field retains up to 259 bytes. Playback behavior and the cause
of the reported overlap remain **unverified** until a symptom-timed live log is
collected and reviewed.
