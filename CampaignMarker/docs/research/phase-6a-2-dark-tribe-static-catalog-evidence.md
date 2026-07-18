# Phase 6A.2 Dark Tribe static catalog evidence

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The exact approved executable contains sufficient static evidence for a
complete 12-entry Dark Tribe catalog. A private mutable menu reader is not the
smallest justified design. The next diagnostic can use an immutable catalog
admitted only for the exact executable and cross-check it against the public
page-21 callback.

This is static design evidence, not implementation, deployment, marker, or
database-write authorization.

## Frozen inputs

- `S4_Main.exe` SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb` SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`;
- Phase 6A.1 live log, 12686712 bytes, SHA-256
  `3450567156fd241417248fe4dd6c476d0ed8d66cbc6f88164cb974bd7d2c9d06`.

The PDB names the Dark Tribe state and its state callbacks, but does not name
a mission array or catalog object. No missing type is invented from the PDB.

## Exact static windows

All RVAs are relative to the preferred image base `0x00400000`. Raw window
hashes are over the exact approved file bytes.

| Purpose | VMA / RVA | File offset | Bytes | SHA-256 |
| --- | --- | ---: | ---: | --- |
| Construct mission controls | `0x47D7B0` / `0x0007D7B0` | `0x7CBB0` | 561 | `44b14df6007177e510e7f68311f7827ead6cf7d9f3ae8271f9daa60cfeeae016` |
| Dispatch control to task index | `0x47DAEB` / `0x0007DAEB` | `0x7CEEB` | 677 | `e0a84e09d81d061ea4a948ee3a97c86dbfc6d74f14a1ce8f138a6ef17964df34` |
| Initialize campaign path formats | `0x4233F0` / `0x000233F0` | `0x227F0` | 384 | `96a78fc162b178d98a4733fc758b0cc1fb1e73ba75ce51bd26b5a4308a6bd978` |
| Dark Tribe UTF-16 format literal | `0x105624C` / `0x00C5624C` | `0xC5564C` | 52 | `a069d33c86107978bfa569361dc176d7387d071d64d64858e49168f7a6214d14` |

The construction window pushes control IDs `0x821..0x82C` (2081..2092) in
order. The click window subtracts `0x820`, bounds the result to 12, and its 12
mission branches pass consecutive zero-based values `0..11` into the same
state-transition constructor. The UTF-16 literal decodes exactly to:

```text
Map\Campaign\dark%02i.map
```

The formatter initializer stores that literal in the Dark Tribe member of the
five-campaign format table. Phase 6A.1 independently anchored control 2083 to
zero-based branch value 2 and same-session SU relative
`Map\Campaign\dark03.map`. The admitted mapping for the other consecutive
branches is therefore a bounded inference from the exact control dispatch and
the exact path formatter, not from label text or save/display names.

## Public geometry anchor

The Phase 6A.1 accepted live run emitted one complete 21-feature public
snapshot at click time. Its 12 mission controls were:

| Mission | Control | Rect | Relative identifier |
| ---: | ---: | --- | --- |
| 1 | 2081 | `500,104,175,30` | `Map\Campaign\dark01.map` |
| 2 | 2082 | `500,136,175,30` | `Map\Campaign\dark02.map` |
| 3 | 2083 | `500,168,175,30` | `Map\Campaign\dark03.map` |
| 4 | 2084 | `500,200,175,30` | `Map\Campaign\dark04.map` |
| 5 | 2085 | `500,232,175,30` | `Map\Campaign\dark05.map` |
| 6 | 2086 | `500,264,175,30` | `Map\Campaign\dark06.map` |
| 7 | 2087 | `500,296,175,30` | `Map\Campaign\dark07.map` |
| 8 | 2088 | `500,328,175,30` | `Map\Campaign\dark08.map` |
| 9 | 2089 | `500,360,175,30` | `Map\Campaign\dark09.map` |
| 10 | 2090 | `500,392,175,30` | `Map\Campaign\dark10.map` |
| 11 | 2091 | `500,424,175,30` | `Map\Campaign\dark11.map` |
| 12 | 2092 | `500,456,175,30` | `Map\Campaign\dark12.map` |

The geometry is a public observation. The relative identifiers are admitted
from the exact static dispatch/format pair, with Mission 3 dynamically
confirmed. Labels such as `Mission 3` are presentation-only and never serve
as identity.

## Evidence boundary

The catalog may be admitted only when the executable compatibility check and
all selected immutable code/literal windows match. Any mismatch disables the
entire catalog. Runtime code must not call the reviewed functions, read their
mutable globals, patch them, synthesize input, or infer identity from a save
name. The existing same-session SU `identity.relative` association remains the
authority for live launch validation.

The locked state of Missions 7 through 12 in this user's profile is not
permission to edit campaign progress or saves. Locked entries are covered by
static evidence and public geometry; only currently selectable missions may be
used in live batch validation.
