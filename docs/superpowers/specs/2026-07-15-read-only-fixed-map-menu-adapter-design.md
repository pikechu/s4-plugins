# Read-Only Fixed-Map Menu Adapter Design

Date: 2026-07-15 (Asia/Shanghai)

## Decision

The public `AddGuiElementBltListener` path is insufficient for immediate and
scroll-stable completion markers. Live testing proved that a fixed-map row's
complete label is delivered only after the mouse enters that row. The public
API has no visible-control enumeration or non-interactive redraw request.

This separately approved design adds a read-only adapter for the fixed-map menu
model. It does not hook a function, patch code, write memory, change page state,
move the cursor, synthesize input, or retain a game-owned pointer.

## Compatibility gate

The adapter is enabled only for the already approved executable:

- version: `2.50.1516.0`;
- SHA-256:
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- PE32 image size at least `0x012bf000`;
- exact instruction windows for list construction, visible-row lookup, and
  scroll-bound updates must match the reviewed bytes in the loaded image.

The reviewed instruction-window image RVAs are `0x0008d660` (visible-row
lookup), `0x0008e700` (scroll-down bound), and `0x001202ae` (construction).
The first two values include the PE `.text` section RVA of `0x1000`; section
offsets such as `0x0008c660` are not image RVAs and must fail closed.

Any mismatch disables only the internal menu adapter. Completion detection,
persistence, backup recovery, and the existing public marker path remain
unchanged.

## Accepted read-only layout

All locations are RVAs from the loaded `S4_Main.exe` base:

| Field | RVA | Bound and meaning |
| --- | ---: | --- |
| scroll base | `0x00e938d8` | first visible sorted-entry index |
| entry array | `0x00e97848` | `1000` bounded 32-bit entry pointers |
| entry count | `0x0109c1d8` | `0..1000` initialized entries |
| array alias | `0x0109c1e0` | must equal image base + entry-array RVA |
| display name | entry `+0x18` | MSVC x86 `std::wstring`, bounded copy |

The reviewed renderer at VMA `0x0048d600` computes each presented item as
`entry_array[scroll_base + row]`. The wheel callback at VMA `0x0048e600`
decrements the scroll base only above zero and increments it only when
`scroll_base + 7 < entry_count`. The accepted marker slots remain the six
calibrated rows `0..5`; the game's seventh list iteration is outside the
accepted Phase 5A row signature and is never marked.

## Snapshot transaction

On the game UI thread and only while the exact page set `{4,22,23,25}` is
active, the adapter:

1. reads the array alias, count, and scroll base;
2. validates their fixed bounds and checked address arithmetic;
3. copies at most six display names through bounded readable-range and SEH
   guards into project-owned fixed buffers;
4. rereads the alias, count, and scroll base;
5. publishes the snapshot only if both header reads are identical.

No pointer into game memory survives the call. A missing entry, malformed
string, concurrent mutation, unreadable range, or arithmetic overflow rejects
the entire snapshot.

## Diagnostic checkpoint

The first candidate is diagnostic-only. It logs a bounded record when the
accepted `(count, scroll base, visible labels)` snapshot changes. It does not
use the internal snapshot for drawing. Live acceptance must prove:

- initial entry reports Aeneas in slot 0 without hovering a map row;
- each scroll reports the same six labels visible on screen in the same order;
- scrolling back reports the original mapping;
- Single, Multiplayer, and Custom tab changes produce their own current list;
- leaving the exact page set stops all internal snapshots;
- no read failure, input regression, hang, or exit failure occurs.

Only after that evidence is GO may a separately reviewed rendering candidate
replace public row-label observations with these read-only snapshots.

## Identity and RD rule

The adapter supplies presentation labels and visible slots only. It never
creates or persists completion identity. Marker eligibility continues to come
from `CompletionMarkerIndex`, built from the validated normalized
`identity.relative` value. A player save/display name such as `RD_PlayerSave`
cannot classify a fixed map as random; a true `RD_...` relative identifier
remains random and hidden from fixed-list markers.
