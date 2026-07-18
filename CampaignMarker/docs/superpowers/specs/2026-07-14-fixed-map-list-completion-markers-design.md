# Fixed-Map List Completion Markers Design

## 1. Goal and scope

Add non-interactive completed-map check marks to the three fixed-map lists in
the Single Player flow:

- Single Player Maps;
- Multiplayer Maps opened from Single Player;
- Custom Maps.

Each completed visible row receives a small check mark at its right edge. The
mark follows the row while the list scrolls, does not participate in mouse hit
testing, and does not change selection, double-click, or map-launch behavior.

A newly committed victory must become visible after the player returns to the
list in the same game process. Existing completion records are loaded at game
startup. Campaign pages, manual marking, random-map display, and online
multiplayer UI are outside this phase.

## 2. Constraints

- Use public S4ModApi UI observations and page-frame callbacks for this phase.
- Do not add an internal menu-model hook unless the public evidence cannot
  produce a unique, safe row association and a separate design is approved.
- Do not modify maps, saves, or game resources.
- Do not read JSON or enumerate the filesystem every frame.
- Do not use hard-coded absolute screen coordinates.
- Preserve completion detection and persistence if marker rendering fails.
- Keep `CompletionMarkers=0` until calibration passes.
- Deployment retains the existing authorization boundary: only this project's
  ASI/configuration and the separately authorized guarded SU archive replacement
  may be changed, and only after the user closes the game and approves deployment.

S4ModApi exposes UI-frame and GUI-element observation callbacks. Its official
HelloWorld example demonstrates `CreateCustomUiElement`, but that API is based
on a fixed screen position and is not the primary renderer for scrolling rows:

- <https://github.com/nyfrk/S4ModApi>
- <https://github.com/nyfrk/S4ModApi/blob/master/Examples/HelloWorld/HelloWorld/dllmain.cpp>

## 3. Architecture

### 3.1 `CompletionMarkerIndex`

`CompletionMarkerIndex` owns an immutable, thread-safe view derived from a
successfully loaded or committed `CompletionStore` snapshot. It contains only
records that are eligible for fixed-list display.

The index is built once after store load. A successful new commit publishes a
replacement snapshot to the index. Rendering reads the immutable view without
waiting for file I/O.

The index accepts a persisted record only when all of these conditions hold:

- `map_kind` is `fixed`;
- `relative_id` is a valid normalized fixed-map path;
- its canonical category is one of `Map\Singleplayer`, `Map\Multiplayer`, or
  `Map\User`;
- its launch source is not `OnlineMultiplayer` or `LoadOnlineMultiplayer`;
- persisted-record marker policy does not suppress it.

Random-map records remain persisted but never enter the marker index.

### 3.2 `FixedMapRowObserver`

`FixedMapRowObserver` consumes GUI-element observations only while the exact
fixed-map page set is active. It uses the existing approved tab attribution:

| Tab | Element ID | Canonical category |
| --- | ---: | --- |
| Single Player Maps | `2449` | `Map\Singleplayer` |
| Multiplayer Maps | `2450` | `Map\Multiplayer` |
| Custom Maps | `2451` | `Map\User` |

For every candidate visible row, the observer records only the current frame's
public properties needed for matching and rendering: normalized row text,
rectangle, surface dimensions, and a bounded control signature. It never
retains raw pointers from the callback.

Observed rows belong to one frame generation. A page-set change, tab change,
or new frame invalidates the preceding generation, so stale marks cannot remain
after navigation or scrolling.

### 3.3 `CompletionMarkerRenderer`

`CompletionMarkerRenderer` receives already matched draw commands. On the
fixed-map page's frame callback it draws all visible checks in one bounded pass.
It creates no custom mouse control and does not change or discard native GUI
elements.

The renderer obtains position from the observed row rectangle and converts it
with the observed surface dimensions plus the frame callback's pillarbox
offset. It does not infer a position from resolution-specific constants.

### 3.4 Data flow

```text
valid store snapshot or committed victory
                |
                v
      immutable marker index
                |
                +----------------------+
                                       |
public visible-row observations        |
                |                      |
                v                      v
       category-bounded unique row match
                         |
                         v
              frame-local draw commands
                         |
                         v
                 UI-frame check marks
```

## 4. Conservative row association

The public GUI observation does not directly expose a map path. The first
fixed-list implementation therefore treats row association as a conservative
presentation lookup, never as completion identity.

### 4.1 Normalization

Row text is decoded to Unicode using the game encoding established by the
calibration run. If the encoding cannot be established without loss, the
calibration is NO-GO. Decoded text receives only these transformations:

- trim leading and trailing whitespace;
- compare UTF-16 text with ordinal, case-insensitive Win32 semantics;
- normalize path filename separators only when comparing a record filename;
- reject embedded control characters, invalid encoding, and oversized text.

The stable database key remains the normalized relative map path. Row text is
never used to insert, merge, or migrate a completion record.

### 4.2 Match requirements

A row produces a check only when all requirements are true:

1. the exact fixed-map page set and one approved tab are active;
2. the normalized row text equals at least one indexed candidate's current
   display name within that tab's canonical category;
3. exactly one such indexed candidate remains after category filtering;
4. that candidate's relative path filename can be uniquely associated with the
   observed name under the calibrated fixed-map naming rule;
5. no second visible row or indexed candidate creates an ambiguity;
6. the row rectangle and surface geometry pass bounds checks.

No match, multiple matches, an encoding error, an unknown tab, or a category
conflict produces no marker. A rate-limited diagnostic identifies the rejection
class without logging pointers or absolute game paths.

This deliberately prefers a missing check over a check on the wrong row. If
calibration shows that public row properties cannot provide this unique
association, implementation stops at the diagnostic result. A direct internal
list-model adapter then requires a new approved design.

## 5. Marker appearance and rendering

The mark is a small green check with a one-pixel dark outline. It is generated
by the plugin rather than loaded from a new external image:

- horizontally inset from the row's right edge;
- vertically centered in the row;
- sized to `clamp(row_height / 2, 12, 16)` logical pixels and inset four
  logical pixels from the right edge; rows too small to contain that geometry
  are rejected;
- clipped to the row and destination surface;
- visible on normal, hover, and selected row backgrounds.

Rendering occurs after the native fixed-list UI for the applicable frame. The
renderer calls `IDirectDrawSurface7::GetDC` once per relevant frame, draws the
outlined checks as bounded GDI polylines, and calls `ReleaseDC` before returning.
Those operations sit behind a small surface abstraction so geometry and failure
behavior can be unit tested without a live DirectDraw surface. The successful
per-frame path performs no allocation, file I/O, JSON parsing, directory
enumeration, or logging.

## 6. Immediate refresh

Initial store loading publishes the first marker index, including a valid
read-only backup snapshot when the main database is unavailable.

`CompletionWorker` publishes a marker update only after
`CompletionAddStatus::Committed`. It does not publish speculative state before
the transaction succeeds. A duplicate does not require a semantic change; a
failed or read-only submission never adds a new in-memory marker.

After a commit, the next fixed-list frame reads the replacement immutable index.
The player does not need to restart the game or force a JSON reload.

## 7. Failure containment and shutdown

- Invalid row evidence fails closed for that row.
- Invalid frame/page evidence drops all draw commands for that frame.
- A drawing-surface failure skips that frame and records a rate-limited error.
- Repeated drawing failures disable only `CompletionMarkerRenderer` for the
  rest of the process and write one terminal marker-renderer error.
- Store corruption and read-only behavior remain governed by the existing
  completion-store policy.
- Exceptions must not cross an S4ModApi callback boundary.
- Stop closes the shared callback gate, waits for in-flight callbacks, removes
  listeners in reverse registration order, and destroys marker state only
  after callbacks are quiescent.

No renderer failure may disable native victory observation or completion
persistence.

## 8. Configuration and diagnostics

`CompletionMarkers` remains the explicit marker feature gate:

```ini
CompletionDetection=1
CompletionStorage=1
CompletionMarkers=0
```

The calibration build keeps the value at `0`. It writes a per-process file named
`marker-calibration-pid-<pid>.trace` below this project's existing
`Plugins\CampaignCompletion` directory. The trace stops accepting records at
256 KiB. It contains only category, bounded row/control features, normalized
map display name, logical rectangle, surface size, frame generation, and
match/rejection class. It contains no raw pointers, module bases, memory
contents, absolute paths, or unrelated UI text.

After calibration passes, the drawing build changes only
`CompletionMarkers=1`. Normal successful rendering does not emit a record per
frame.

## 9. Automated verification

Pure and component tests must cover:

- index filtering for fixed, random, fixed offline, and online sources;
- category derivation for all three fixed-map directories;
- Unicode/text bounds and invalid encoding rejection;
- unique match, no match, duplicate name, duplicate row, and category conflict;
- geometry conversion, clipping, minimum/maximum size, and pillarbox offset;
- generation invalidation on frame, tab, and page-set changes;
- initial loaded snapshot and read-only-backup display;
- immediate publication only after a committed completion transaction;
- failure isolation and idempotent renderer disable;
- callback shutdown ordering and no use of stale observations.

The existing completion, native-event, store corruption, load-game, random-map,
and shutdown tests remain mandatory regression gates. Win32 CI must build the
Release x86 ASI and verify package contents and configuration flags.

## 10. Live calibration and acceptance

### 10.1 Calibration build (`CompletionMarkers=0`)

With a fresh process, the user:

1. opens Single Player Maps and locates completed map Aeneas;
2. observes Aeneas in normal, hover, and selected states;
3. scrolls Aeneas out of view and back into view;
4. opens Custom Maps and repeats the sequence for completed map Antares;
5. opens Multiplayer Maps as a no-completed-record negative control;
6. returns normally and closes the game.

The calibration is GO only when each target produces one stable, category-correct
row association across states and scrolling, no other row matches, callback
input remains responsive, and shutdown is clean.

### 10.2 Drawing build (`CompletionMarkers=1`)

The user repeats the three-list navigation and verifies:

- Aeneas and Antares display one correctly positioned check each;
- checks track their rows during scrolling;
- hover, selection, single-click, double-click, Back, and map launch respond
  normally;
- Multiplayer Maps shows no false marker;
- returning to menus and exiting the game remains clean.

For immediate-refresh acceptance, the user then wins one previously unrecorded
fixed map through an approved normal test flow and returns to its list without
restarting the game. The new row must show its check, the database must contain
one new stable record, and no other row may change.

Any wrong-row check, stale check, scroll drift, input regression, exit error, or
renderer diagnostic failure is NO-GO. The feature remains or returns to
`CompletionMarkers=0` while detection and persistence stay enabled.

## 11. Deferred work

The following require later designs:

- campaign-node and campaign-mission-list adapters;
- a direct internal fixed-list model adapter if public matching fails;
- manual completion correction and hotkeys;
- configurable marker art, position, or color;
- showing random-map records in any UI.
