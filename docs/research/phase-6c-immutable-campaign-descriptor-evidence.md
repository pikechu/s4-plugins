# Phase 6C immutable campaign descriptor evidence

Date: 2026-07-16 (Asia/Shanghai)

## Scope and authority

This is offline static evidence for a read-only diagnostic catalog. It does
not authorize deployment, marker rendering, completion reads or writes,
database access, save access, input synthesis, internal function calls, or
campaign-progress changes. Live classification remains exclusively the exact
confirmed same-MapInit-session `identity.relative` value.

The frozen inputs are:

- `S4_Main.exe` version `2.50.1516.0`, SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb`, SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`;
- PE preferred image base `0x00400000`;
- Phase 6B public catalog observations and the previously accepted Dark Tribe
  same-session anchor `2083 -> Map\Campaign\dark03.map`.

Display labels, `identity.name`, completion JSON fields, filenames, and any
string containing `RD` were excluded from every mapping decision.

## Frozen admission windows

The production diagnostic first requires the exact executable identity above,
then compares these bounded live image windows. A mismatch disables only the
affected group.

| Group | Purpose | RVA | Length | Bytes | SHA-256 |
| --- | --- | ---: | ---: | --- | --- |
| Add-on | Trojan control normalization `91..102` | `0x0006B27B` | 12 | `0fb7450c83c0a683f80c0f87` | `d04c8be2a41b9b3fb52d2005972cacb6f26aa515222eab890b306c26ffba19b4` |
| Add-on | Mayan control normalization `34..37` | `0x0006A25B` | 12 | `0fb7450c83c0df83f8040f87` | `0aff7a541bec5c6a06a1fa705d72f06358f7936f8d4984e9f1608626495e7986` |
| Mission CD | Mayan constructor branch | `0x00095E36` | 8 | `85c90f8408010000` | `a50f992f0ceb2f0d32c48e8b03d12c1ad4f0a6cebf2a63dd8984e7ec497888e0` |
| Mission CD | Roman constructor branch | `0x000961F6` | 8 | `85c90f8408010000` | `a50f992f0ceb2f0d32c48e8b03d12c1ad4f0a6cebf2a63dd8984e7ec497888e0` |
| Original | `CStateCampaign3X3` constructor branch | `0x0007BF96` | 8 | `85c90f84a2010000` | `e8be1aba33c64d94d5a76c3cfe30e6c41aa5e1e8a2684e88091727fdfc44c078` |
| Original | control normalization from `2037` | `0x0007C22B` | 14 | `0fb7450c050bf8ffff83f8090f87` | `3b196a6d588966454e107e3ad485ed0d18180a90659af646ea9f84350a511c36` |
| Dark Tribe | mission-control constructor branch | `0x0007D7B6` | 8 | `85c90f8422020000` | `aa69a80f76309921de1b4674e77206dabe1fc1468e370396a67fee5bfb86b053` |
| Dark Tribe | control dispatch | `0x0007DAEB` | 4 | `0fb7450c` | `c8517efb3ef3407b04ffe6c1c30a9358b7890b35fdbc61ed796151a19f5d9774` |

These short windows are defense in depth. The complete immutable chain was
reviewed in disassembly: bounded constructor IDs, jump-table branch ordinal,
transition state and formatter slot, and the exact format literal initialized
by the frozen executable. Runtime never calls those functions or reads their
mutable campaign state.

## Admitted records

Only records with both a complete static chain and an exact Phase 6B public
rectangle are compiled into the candidate.

| Decision | Group/page | Public control and rectangle | Ordinal / slot | Exact relative identifier |
| --- | --- | --- | --- | --- |
| STATIC READY | Add-on Trojan | `91`, `320,200,175,30` | `0 / 0x0F` | `Map\Campaign\ao_trojan01.map` |
| STATIC READY | Add-on Mayan | `34`, `480,177,175,30` | `0 / 0x0D` | `Map\Campaign\ao_maya1.map` |
| STATIC READY | Mission CD Roman | `1903`, `237,148,175,30` | `0 / 0x11` | `Map\Campaign\mcd2_roman1.map` |
| STATIC READY | Mission CD Roman | `1904`, `237,195,175,30` | `1 / 0x11` | `Map\Campaign\mcd2_roman2.map` |
| STATIC READY | Mission CD Mayan | `1889`, `430,177,175,30` | `0 / 0x13` | `Map\Campaign\mcd2_maya1.map` |
| STATIC READY | Original Viking | `2039`, `420,150,175,30` | `1 / 0x01` | `Map\Campaign\viking02.map` |
| STATIC READY | Original Viking | `2040`, `420,190,175,30` | `2 / 0x01` | `Map\Campaign\viking03.map` |
| STATIC READY | Dark Tribe | `2081..2092`, `500,104+32*n,175,30` | `0..11 / 0x03` | `Map\Campaign\dark01.map` through `dark12.map` |

The Dark Tribe range reuses the accepted anchor and the previously frozen full
constructor/dispatch/formatter proof. The other ready entries are deliberately
only those actually observed with stable public geometry in Phase 6B.

## Evidence gaps and fail-closed decisions

- Add-on Roman, Viking, Settle, and Bonus: candidate static ranges exist, but
  no complete stable public mission rectangle was observed. `EVIDENCE GAP`.
- Mission CD Viking and Conflict: static transition families exist, but the
  public mission controls were not observed. `EVIDENCE GAP`.
- Other Original campaign controls: the combined page is state-dependent and
  the observed geometry/branch subset is incomplete. `EVIDENCE GAP`.
- New World and New World 2 / Great Crusades: formatter families are visible
  statically, but no public mission control-to-ordinal chain is closed.
  Both groups are unconditionally disabled. `EVIDENCE GAP`.

A plausible prefix, translated label, page position, or sibling formatter is
never promoted into a record.

## Minimal dynamic anchor manifest

One uninterrupted future live batch is sufficient for the newly admitted
transition implementations:

1. Add-on: control `91`, expect exactly
   `Map\Campaign\ao_trojan01.map`;
2. Mission CD: control `1903`, expect exactly
   `Map\Campaign\mcd2_roman1.map`;
3. Original: control `2039`, expect exactly
   `Map\Campaign\viking02.map`.

Dark Tribe needs no repeated launch because its unchanged association chain is
already anchored. If any representative is locked or unavailable, that group
remains dynamically unanchored; no save, database, or campaign-progress change
is permitted. Marker behavior remains outside Phase 6C.
