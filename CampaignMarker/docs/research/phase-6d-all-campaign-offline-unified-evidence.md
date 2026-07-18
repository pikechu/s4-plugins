# Phase 6D all-campaign offline unified evidence

Date: 2026-07-16 (Asia/Shanghai)

> Superseded for geometry status on 2026-07-18: the approved gap-only live
> batch closed all 107 public rectangles with zero remaining evidence gaps.
> See
> [the Phase 6D geometry report](phase-6d-gap-only-all-campaign-geometry-report.md).
> This document remains the frozen static control-to-relative evidence.

## Decision

Offline analysis closes the exact control-to-relative chain for 107 selectable
campaign mission controls in the accepted executable. Nineteen rows also have
accepted public geometry and are `CLOSED`; the remaining 88 rows are
`EVIDENCE GAP` only because their logical public rectangles were not retained
by the frozen sparse catalog logs.

Phase 6D is therefore **NO-GO for all-campaign implementation** at this
checkpoint. The next efficient step is one gap-only public catalog candidate
that emits every mission control and rectangle without hover, followed by one
continuous traversal. No family-by-family mission launches or victories are
needed.

This report is offline evidence only. It did not read the completion database
or saves, start or control a process, deploy an archive, modify the installed
game, or classify any runtime identity. The installed Phase 6C.1 candidate is
unchanged.

The row-level artifact is
[`research/evidence/phase-6d-all-campaign-descriptor-matrix.tsv`](../../research/evidence/phase-6d-all-campaign-descriptor-matrix.tsv).

## Frozen inputs

- `S4_Main.exe`: SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb`: SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`;
- initial Phase 6B catalog slice: log offsets
  `[12686712,12741849)`, 55137 bytes, SHA-256
  `60d2c47a785c91f237d9c080458d6f6f86df7eaed3bee47175ab70ba66acfb93`;
- focused composed-page slice: log offsets
  `[12792772,12833244)`, 40472 bytes, SHA-256
  `869580203b3422031a5681a86fd0a52fb9af9910c7b177f8eaac21e7b38c3967`;
- reusable Dark Tribe evidence:
  [Phase 6A.2](phase-6a-2-dark-tribe-static-catalog-evidence.md);
- corrected `md_roman*` evidence:
  [Phase 6C.1](phase-6c-1-descriptor-family-path-evidence.md).

No bytes outside the approved executable, PDB, frozen log ranges, and existing
workspace evidence were used to assign a descriptor.

## Status summary

| Content | Rows | CLOSED | EVIDENCE GAP | Missing fact |
| --- | ---: | ---: | ---: | --- |
| Add-on child pages 11–15 | 28 | 2 | 26 | public rectangle |
| Mission CD child pages 16–19 | 21 | 3 | 18 | public rectangle |
| Mission CD bonus on selector page 4 | 1 | 0 | 1 | public rectangle |
| New World / MCD2 composed owner | 20 | 0 | 20 | public rectangle |
| New World 2 / XMD3 composed owner | 16 | 0 | 16 | public rectangle |
| Original page 20 | 9 | 2 | 7 | public rectangle |
| Dark Tribe page 21 | 12 | 12 | 0 | — |
| **Total** | **107** | **19** | **88** | |

Every gap row already has an exact control ID, zero-based ordinal, event,
transition kind, formatter selection, and exact relative. No gap is being
filled from a label, apparent ordering, sibling filename, or translated text.

## Static window identifiers

All RVAs are relative to image base `0x00400000`. File offsets use the
accepted PE32 `.text` mapping. The broad UI/state windows deliberately bind
the complete grouped construction/dispatch/consumer chains; the existing Dark
and Phase 6C.1 reports retain their smaller compatibility windows.

| ID | Purpose | VMA range | File offset | Length | SHA-256 |
| --- | --- | --- | ---: | ---: | --- |
| AO | child constructors/dispatch plus selector; state consumers | `0x46A1F0..0x46B930`; `0x50DDA0..0x5100A0` | `0x695F0`; `0x10D1A0` | 5952; 8960 | `f096f8969a8304498b0cd053adbb6c5a7793e77eb317f9c591b4c41f67dcb36d`; `09408e191881efa8ba44705bb66a358e3e83d3e362d288ffed6a3596e4b98971` |
| MD | child/selector constructors and dispatch; state consumers | `0x495E30..0x497240`; `0x524350..0x525F10` | `0x95230`; `0x123750` | 5136; 7104 | `147f9db65cbdd0b09fba7a0001cb15c96bea8cba4855226c6ebfca98c59b90b3`; `c34714420d42d8f680705dd2d88dd158eb92ace78bcbe2b601a7eba8788e141b` |
| MD2 | composed UI dispatch; state/formatter | `0x495270..0x495680`; `0x5234F0..0x524220` | `0x94670`; `0x1228F0` | 1040; 3376 | `85f8bbd64886596ca14e3e9f5e624c91b30785331e2e4b243abedf33455d1e47`; `12d6217ca1e1354ac4ff4b9b2d8dbf521c743bcdf5e3205f20c090b59d38372a` |
| XMD3 | composed UI dispatch; state/formatter | `0x4AFF80..0x4B0A40`; `0x528AE0..0x5298C4` | `0xAF380`; `0x127EE0` | 2752; 3556 | `e1374d0d00cdd2d2280ea963907cf6f9692198c449862fd59895e80e2e3c4b50`; `08595043d6f859af73f4f92311023d60b1da1ebaf86f80c2e47b556741945471` |
| ORIG | 3x3 UI dispatch; state/formatter | `0x47C150..0x47C580`; `0x510610..0x5114B7` | `0x7B550`; `0x10FA10` | 1072; 3751 | `ded0825cc53c46669bc2b8ba8e2bf19085b81129554609e81d035ce724d769b3`; `685185042a2f72803083ffc09df07b225812a2a8569704e78e3908b9e3e95722` |
| DARK | exact constructor and click dispatch | `0x47D7B0` (561 bytes); `0x47DAEB` (677 bytes) | `0x7CBB0`; `0x7CEEB` | 561; 677 | `44b14df6007177e510e7f68311f7827ead6cf7d9f3ae8271f9daa60cfeeae016`; `e0a84e09d81d061ea4a948ee3a97c86dbfc6d74f14a1ce8f138a6ef17964df34` |

The five formatter initializer hashes remain:

| Formatter family | VMA | Length | SHA-256 |
| --- | ---: | ---: | --- |
| Add-on | `0x423090` | 415 | `9459c9d0fc6c027ec736eb9bf9b3c0ddf6642edc0e8957919406bc8f81812799` |
| Original | `0x4233F0` | 384 | `96a78fc162b178d98a4733fc758b0cc1fb1e73ba75ce51bd26b5a4308a6bd978` |
| MCD2 | `0x423980` | 297 | `d37b56be5c1509afac177a56b17f9a9849b1ebf25cc696de848fb6d74a1c5c56` |
| Mission CD | `0x423AB0` | 415 | `63278d29a609391708defde9fbbf28fd9c09d9f8a1ac9521fd8beab849aef8b8` |
| XMD3 | `0x423CA0` | 297 | `3c561626b14678bf2a94d76097c44b785a91dd520a0e6d4438cecb6e1576ce88` |

## Exact dispatch closure

The grouped control flow proves these mappings:

| Public content | Controls | Event | Transition kind | Exact formatter |
| --- | --- | --- | --- | --- |
| Add-on Roman | 47–50 | `0x1F49` | `0x0B` | `ao_roman%01i` |
| Add-on Viking | 112–115 | `0x1F4D` | `0x0C` | `ao_viking%01i` |
| Add-on Mayan | 34–37 | `0x1F4B` | `0x0D` | `ao_maya%01i` |
| Add-on Settle | 77–80 | `0x1F47` | `0x0E` | `ao_settle%01i` |
| Add-on Trojan | 91–102 | `0x1F4F` | `0x0F` | `ao_trojan%02i` |
| Mission CD Roman | 1903–1907 | `0x1B60` | `0x05` | `md_roman%01i` |
| Mission CD Viking | 1950–1954 | `0x1B64` | `0x06` | `md_viking%01i` |
| Mission CD Mayan | 1889–1893 | `0x1B62` | `0x07` | `md_maya%01i` |
| Mission CD Settle | 1941–1943 | `0x1B66` | `0x08` | `md_settle%01i` |
| Mission CD Conflict | 1944–1946 | `0x1B67` | `0x09` | `md_conflict%01i` |
| Mission CD Bonus | 1919 | `0x1B5F` | `0x0A` | exact `md_bonus` literal |
| New World / MCD2 Roman, Viking, Mayan, Trojan | 1825–1844 in four five-entry runs | `0x232A..0x232D` | `0x11..0x14` | `mcd2_*` in Roman/Viking/Mayan/Trojan order |
| New World 2 / XMD3 Roman, Viking, Trojan, Mayan controls | 2939–2954 in four four-entry runs | `0x251E`, `0x251F`, `0x2521`, `0x2520` | `0x15`, `0x16`, `0x18`, `0x17` | transition kinds index `xmd3_*` in Roman/Viking/Trojan/Mayan control order |
| Original Viking, Mayan, Roman | 2038–2046 in three three-entry runs | `0x66` | `0x02`, `0x03`, `0x01` | `viking%02i`, `maya%02i`, `roman%02i` |
| Dark Tribe | 2081–2092 | `0x64` | `0x04` | `dark%02i` |

The matrix's “transition kind” is the low-word state value. Existing production
records call the zero-based table selection a `formatterSlot`; for example,
Original Viking transition kind `0x02` selects formatter slot 1, and Dark
Tribe kind `0x04` selects formatter slot 3. Those are not contradictions.

## Corrections and exclusions

### The historical `mcd2_*` association is superseded

Controls 1903–1907 consume event `0x1B60`, select kind `0x05`, and index the
`md_roman*` table. The same pattern closes pages 17–19 under `md_*`.
The separate controls 1825–1844 select kinds `0x11..0x14` and index the
`mcd2_*` table. Therefore `mcd2_*` belongs to the composed New World path,
not pages 16–19.

### Selector page 4 contains one mission identity

The approved design described selectors 3/4 as navigation-only. Exact control
flow proves one exception: control 1919 on page 4 emits `0x1B5F`, selects
transition kind `0x0A`, and formats the exact fixed relative
`Map\Campaign\md_bonus.map`. It is included as row `md-bonus` with a
geometry gap. The approved design must be corrected before implementation;
this offline phase does not silently change production behavior.

### Non-campaign 12-entry handler excluded

Controls 2798–2809 dispatch ordinals 0–11, but their exact formatter is
`Map\Tutorial\Tutorial%02i.map`. They are tutorial missions, not an
unmapped `troja*` campaign. No display label was used for this exclusion.

### Unavailable legacy formatter content

- `ao_bonus` exists as an initialized literal, but Add-on selector control 76
  emits `0x1F48` and the exact selector consumer routes it to the rejecting
  default rather than `CStateAOCampaignBonus`.
- `troja%02i` exists in the Original formatter table, but the complete exact
  executable has no caller selecting transition kind 5 for the common Original
  formatter. Its only explicit state transitions select kinds 1–3 (3x3) and 4
  (Dark Tribe), and the accepted catalog sweep exposed no Trojan control.

Neither legacy family receives invented descriptor rows or relatives. They are
recorded as unavailable for this exact executable, not as failed or locked
missions.

## Public geometry evidence and remaining gap

Source IDs in the TSV mean:

- `L6B-I`: the initial Phase 6B frozen slice;
- `L6B-F`: the focused composed-page frozen slice;
- `D6A`: the accepted complete Phase 6A.1 page-21 snapshot frozen by the
  Phase 6A.2 report.

The Phase 6B diagnostic retained only sparse changed/hover-related features.
It proves geometry for Add-on Trojan 1, Add-on Mayan 1, Mission CD Roman 1/2,
Mission CD Mayan 1, Original Viking 2/3, and navigation controls. It does not
prove that unobserved mission controls are unavailable. The composed slice
retained only navigation/effect controls 1826/1827/2938 and no mission
rectangle. Absence is therefore recorded as a geometry evidence gap.

A single revised capture can close all 88 gaps by enumerating the complete
read-only public snapshot at each stable page/scroll epoch. It must not derive
rectangles from neighboring buttons, text, control ordering, or visual
patterns. The pass can stay at menus: no hover, launch, victory, database read,
save read, or campaign-progress change is required.

## Strict identity and RD boundary

The static relatives in this report constrain expected descriptors only.
A future live launch is admitted solely when the same fresh MapInit session
confirms an exact `identity.relative` equal to the selected descriptor.
`identity.name`, labels, translated text, save names, database display
fields, and any presentation string containing `RD` remain non-authoritative.

Only a confirmed same-session relative beginning with `RD_` is random. No row
in this matrix and no presentation field can override that rule.

## Checkpoint boundary

This checkpoint authorizes no production catalog expansion, marker rendering,
database access, deployment, or process action. The all-campaign implementation
remains blocked until:

1. the page-4 bonus exception is incorporated into an approved design
   correction;
2. all 88 public rectangles are captured and the matrix contains no
   `EVIDENCE GAP`;
3. the frozen result is reviewed before RED implementation begins.
