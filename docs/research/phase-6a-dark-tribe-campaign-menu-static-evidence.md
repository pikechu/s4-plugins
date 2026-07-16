# Phase 6A Dark Tribe campaign-menu static evidence

Date: 2026-07-16 (Asia/Shanghai)

## Frozen identities

The protected manifest identifies the approved inputs as:

- `S4_Main.exe` SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb` SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`;
- public `S4ModApi.h` SHA-256
  `31069630faffea89d21359bdc001261fbf4eef3307b7ee900be4d44934a835d5`.

These are compatibility evidence, not authorization to call internal
functions or read private layouts.

## Public page and callback evidence

In the approved public header, enum ordering makes
`S4_SCREEN_SINGLEPLAYER_DARKTRIBE` page 21. The GUI-element callback type is:

```cpp
typedef HRESULT(FAR S4HCALL* LPS4GUIDRAWCALLBACK)(
    LPS4GUIDRAWBLTPARAMS entity, BOOL discard);
```

`S4GuiElementBltParams` exposes geometry, texture/style fields, `valueLink`,
and game-owned text pointers, but it contains no screen/page field. Therefore
Phase 6A admits page ownership only through the page-specific frame callback
and `IsCurrentlyOnScreen(21)`, and copies callback values immediately into
bounded project-owned storage.

## PDB navigation records and reviewed windows

The normalized approved PDB records contain:

| Symbol | CodeView section:offset | Image RVA | VMA |
|---|---:|---:|---:|
| `CStateCampaignDark::sub_111540` | `0001:1115456` (`0x110540`) | `0x111540` | `0x511540` |
| `CStateCampaignDark::sub_1116F0` | `0001:1115888` (`0x1106F0`) | `0x1116F0` | `0x5116F0` |
| `CStateCampaignDark::sub_1117F0` | `0001:1116144` (`0x1107F0`) | `0x1117F0` | `0x5117F0` |

The bounded disassembly window `0x511540..0x511880` was reviewed against the
approved EXE. It contains ordinary state construction/destruction setup around
`0x511540..0x5116EF`; the callback beginning at `0x5116F0` dispatches on input
fields and includes a comparison with `0x1B` at `0x511793`. This is only
navigation/context evidence. Phase 6A neither calls these functions nor
derives a private structure layout from them.

## Frozen historical live excerpt

The prior read-only log
`artifacts/phase-5a-row-calibration/live/pid-31908/CampaignCompletion-pid-31908.log`
contains the following user-driven observations:

```text
2026-07-13T12:39:40.778+LOCAL [INFO] ui-frame page=21
2026-07-13T12:39:41.801+LOCAL [INFO] ui-frame page=21
2026-07-13T12:39:44.340+LOCAL [INFO] gui-elements page=21 count=31 last-rect=512,516,43,65 value-link=2080 texture=65
2026-07-13T12:39:45.177+LOCAL [INFO] mouse page=21 button=1 message=513 x=1385 y=1127 element-id=2080 rect=512,516,43,65
2026-07-13T12:39:45.542+LOCAL [INFO] gui-elements page=21 count=19 last-rect=512,516,43,65 value-link=2403 texture=65
```

This confirms public observability, but the old one-second sampling is not a
mission-control mapping and cannot establish an exact map identity.

## Runtime boundary

The Phase 6A production startup constructs only the bounded campaign-menu
capture/association, public listeners, launch-origin tracker, and Settlers
United Lua identity coordinator. It does not construct or start completion
storage, a completion worker/admission path, native victory subscription,
fixed-map internal reader, or any marker renderer. The INI correspondingly
sets completion detection/storage/markers, native subscription, and internal
rendering to zero.

The diagnostic association emits only the exact same-session confirmed
`identity.relative`. Display/save names—including names beginning `RD` or
`RD_`—never create, replace, or classify identity.

## GREEN checkpoint and audited candidate

The Phase 6A implementation is frozen by commits `3a76792` and `f02805c`.
Authoritative Windows Release workflow run `29476757736` completed successfully
for exact head `f02805c704612b55aac81de61582a7a152a2a9aa`. Configure, build,
`/W4 /WX`, archive integration, source/binary policy, all mutation fixtures,
tests, packaging, PE32/i386 verification, exact package contents, and artifact
upload all passed.

The independently downloaded candidate is frozen as:

- artifact ID `8366884532`, name `CampaignCompletionDebug-Win32`, size
  `235991` bytes, GitHub digest and downloaded SHA-256
  `1979e4168d37af6ccfe8c110c9df2779cd27ac7002e381bb9fd62e463045fbb8`;
- embedded `CampaignCompletionDebug.zip` SHA-256
  `3904bc322e18410f8ab640bf6b2f7e9dd04d5c8860431e5c1fad7c64af688838`;
- `Plugins/CampaignCompletionDebug.asi`, `420864` bytes, SHA-256
  `ddae1357af8f6fd3bb0eaee0c314baff24b95281080da52f1da777820aded617`;
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`, `822` bytes,
  SHA-256
  `00facab7bdf7b70f6f6f20f72ece60dd381e1ffb4e0fdd57ab820a081d2bb5e7`.

The inner package has exactly those two entries. Independent inspection
identifies the ASI as PE32/i386 and exactly one exported
`CampaignCompletionDebugStop`; the packaged INI equals the committed INI after
normalizing the Windows checkout's CRLF line endings. The candidate is audited
but not deployed. Guarded deployment still requires closed game/SU processes
and fresh explicit user approval.
