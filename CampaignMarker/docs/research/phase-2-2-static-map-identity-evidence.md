# Phase 2.2 Static Map Identity Evidence

## Decision

**REJECTED FOR INTERNAL READ-ONLY PROBE.** The reviewed executable contains
three useful but non-equivalent objects:

1. a fixed-map menu entry with an ANSI path at offset `0x00`;
2. a persistent game-settings allocation whose first field is a
   `std::wstring`; and
3. a temporary `S4::CMapFile` whose path field is a `std::wstring` at offset
   `0x50`.

No reviewed instruction connects the selected fixed-map entry's ANSI path to
the game-settings root or leaves a `CMapFile` reachable from a fixed global
after map initialization. The only statically rooted, heap-owned `CMapFile`
found belongs to `CRandomMaps`, which is explicitly outside the approved fixed-
map scope. Combining fields from these different objects would be a guessed
layout and therefore fails the evidence gate.

Consequently, Tasks 2-4 and 6-8 of the approved plan must be skipped. Phase 2.2
may continue only with the public-UI attribution work in Task 5 and must finish
as `PARTIAL/NO-GO` for loaded fixed-map identity unless a separately designed
and approved observation method supplies new evidence.

## Reproduction and Input Gate

The collector is
`research/scripts/collect_map_identity_candidates.sh`. It reads the installed
EXE/PDB and existing symbol evidence, verifies these hashes, and writes only
`research/evidence/map-identity-candidates.txt`:

- `S4_Main.exe`: `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`
- `S4_MainR.pdb`: `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`
- `pdb-symbols.txt`: `dafc72d237209e3c332f44dca44e9fcd24eb49e8b667172ff2eda695c2bc247f`

The EXE and PDB hashes are also required to occur with their exact absolute
paths in `manifest.sha256`. Two consecutive collector runs produced an empty
normalized diff after removing only `generated_utc` and `workspace_commit`.

The raw PE section headers establish these admitted virtual RVA ranges:

- `.text`: `0x00001000..0x00B14EF0`
- `.rdata`: `0x00B15000..0x00D4503A`
- `.data`: `0x00D46000..0x01208F40`

The image base is `0x00400000`. CodeView addresses were converted with
`RVA = section.VirtualAddress + offset`; VMAs below are `image base + RVA`.
All code rows fall in `.text`, `0x00C2DA98` falls in `.rdata`, and all candidate
global storage RVAs fall in the virtual `.data` range.

## Reviewed Instruction Table

The table records only operands present in the captured instructions. “Symbol
RVA” is `n/a` where the stripped public PDB supplies no symbol for the
containing function.

| symbol | symbol RVA | containing function range | instruction VMA/RVA | instruction bytes | decoded operand | target section | inferred ownership | runtime invariant | accepted/rejected reason |
|---|---:|---|---|---|---|---|---|---|---|
| `S4::CMapFile::sub_B11B0` / `??_7CMapFile@S4@@6B@` | `0x000B11B0` / `0x00C2DA98` | `0x004B10D0..0x004B11AE` | `0x004B10FF` / `0x000B10FF` | `c7 07 98 da 02 01` | `[edi] = 0x0102DA98` | `.rdata` target | `edi` is the constructed `CMapFile` | object first word must equal the PDB-derived vtable VMA | Accepted only as the `CMapFile` type discriminator; it does not provide a root. |
| `S4::CMapFile` load | `n/a` | `0x004B1310..0x004B145C` | `0x004B131B` / `0x000B131B` | `8d 4e 50` | `ecx = object + 0x50` | `.text` | inline `std::wstring` member | `CMapFile + 0x50` must have the MSVC wide-string layout | Accepted as `CMapFile` path offset and storage kind only. |
| fixed-map load wrapper | `n/a` | `0x004FEEF0..0x004FF432` | `0x004FEF6E` / `0x000FEF6E` | `8d 8d 48 fb ff ff` | `ecx = ebp - 0x4B8` | stack | local `CMapFile` storage | address is inside the current stack frame | Rejected as a persistent root. |
| fixed-map load wrapper | `n/a` | `0x004FEEF0..0x004FF432` | `0x004FEF77` / `0x000FEF77` | `e8 54 21 fb ff` | call `0x004B10D0` | `.text` | constructs the stack-local `CMapFile` | local vtable becomes `0x0102DA98` | Confirms type, but lifetime is local. |
| fixed-map load wrapper | `n/a` | `0x004FEEF0..0x004FF432` | `0x004FEF9B` / `0x000FEF9B` | `8d 45 d4` | push address of converted input path | stack | temporary path argument | path exists only for this call | Does not expose a global loaded-map root. |
| fixed-map load wrapper | `n/a` | `0x004FEEF0..0x004FF432` | `0x004FF39D` / `0x000FF39D` | `e8 ae 1e fb ff` | call `CMapFile` destructor `0x004B1250` | `.text` | destroys `ebp - 0x4B8` | same local is destroyed before return | Rejects the temporary `CMapFile` as a post-init observation object. |
| game-settings constructor | `n/a` | `0x00506020..0x0050631D` | `0x00506049` / `0x00106049` | `c7 46 10 00 00 00 00` | `[esi + 0x10] = 0` | heap object | first `std::wstring` length | length is zero after construction | Accepted as evidence that offset `0x00` begins an inline MSVC `std::wstring`; not a `CMapFile`. |
| game-settings constructor | `n/a` | `0x00506020..0x0050631D` | `0x00506057` / `0x00106057` | `c7 46 14 07 00 00 00` | `[esi + 0x14] = 7` | heap object | first `std::wstring` capacity | capacity is the wide SSO value `7` | Confirms wide-string storage at object offset `0x00`. |
| single-player lobby start | `n/a` | `0x00526000..0x005268DD` | `0x0052611F` / `0x0012611F` | `68 ac 05 00 00` | allocate `0x5AC` bytes | heap | game-settings allocation | allocation must be non-null before constructor | Accepted allocation size only. |
| single-player lobby start | `n/a` | `0x00526000..0x005268DD` | `0x0052614D` / `0x0012614D` | `a3 a0 68 29 01` | `[0x012968A0] = eax` | `.data` | global pointer owns game-settings allocation | root may be null outside the lobby/session lifecycle | Accepted as game-settings root pointer RVA `0x00E968A0`, but not as a `CMapFile` root. |
| `CStateLobbyMapSettings::Callbacks` | `0x00120C90` | `0x00520C90..0x0052141D` | `0x00520ED0` / `0x00120ED0` | `8b 0d a0 68 29 01` | `ecx = [0x012968A0]` | `.data` | game-settings destination | root must be non-null | Accepted root dereference. |
| `CStateLobbyMapSettings::Callbacks` | `0x00120C90` | `0x00520C90..0x0052141D` | `0x00520EE2` / `0x00120EE2` | `68 1c c2 49 01` | push source `0x0149C21C` | `.data` | global source `std::wstring` | source must satisfy MSVC wide-string invariants | Shows copying the global wide string into game-settings offset `0x00`; it does not prove that source equals the selected entry path. |
| fixed-map list construction | `n/a` | `0x005201E0..0x005207DF` | `0x005202B9` / `0x001202B9` | `89 34 85 48 78 29 01` | `[0x01297848 + index*4] = esi` | `.data` | array owns `0x70`-byte menu entries | index must be within the bounded list capacity | Accepted menu-list root RVA `0x00E97848`. |
| fixed-map list construction | `n/a` | `0x005201E0..0x005207DF` | `0x0052069D` / `0x0012069D` | `8b 34 8d 48 78 29 01` | `esi = [0x01297848 + index*4]` | `.data` | selected entry destination | entry must be non-null | Accepted one menu-entry dereference. |
| fixed-map list construction | `n/a` | `0x005201E0..0x005207DF` | `0x005206D6` / `0x001206D6` | `8b ce` | `ecx = esi` before ANSI string assignment | heap entry | entry offset `0x00` is an MSVC `std::string` | capacity at `+0x14` must be at least length at `+0x10` | Accepted as a menu-only ANSI path candidate. |
| `CStateLobbyMapSettings::Callbacks` | `0x00120C90` | `0x00520C90..0x0052141D` | `0x00520E23` / `0x00120E23` | `a1 ec c1 49 01` | `eax = [0x0149C1EC]` | `.data` | current menu index | index must be within current list count | Accepted selected-index RVA `0x0109C1EC` for menu attribution only. |
| `CStateLobbyMapSettings::Callbacks` | `0x00120C90` | `0x00520C90..0x0052141D` | `0x00520E30` / `0x00120E30` | `8b 04 85 48 78 29 01` | `eax = [0x01297848 + index*4]` | `.data` | selected menu entry | entry must be non-null | Accepted menu lookup. |
| `CStateLobbyMapSettings::Callbacks` | `0x00120C90` | `0x00520C90..0x0052141D` | `0x00520E37` / `0x00120E37` | `83 c0 18` | `eax = entry + 0x18` | heap entry | display-name `std::wstring` | wide-string invariants must hold | The callback consumes the display name, not the ANSI path at `+0x00`; no bridge to the load path is proven. |
| fixed-map list destruction | `n/a` | `0x00520BC0..0x00520C06` | `0x00520BC2` / `0x00120BC2` | `be 48 78 29 01` | `esi = 0x01297848` | `.data` | walks the menu array | entries may be null | Rejected for post-init use because this function destroys and clears every menu entry. |
| `CRandomMaps` owner | `n/a` | `0x0050B1D0..0x0050B28C` | `0x0050B234` / `0x0010B234` | `68 84 04 00 00` | allocate `0x484` bytes | heap | `CMapFile` allocation | allocation must be non-null | Structurally valid `CMapFile` ownership. |
| `CRandomMaps` owner | `n/a` | `0x0050B1D0..0x0050B28C` | `0x0050B262` / `0x0010B262` | `89 47 10` | `[edi + 0x10] = eax` | heap object rooted at `.data` RVA `0x00DCDA70` | owner retains `CMapFile*` | owner vtable/RTTI must identify `CRandomMaps` | Rejected because RTTI resolves to `.?AVCRandomMaps@@`; random maps are outside scope. |

## Candidate Chains

### A. Temporary fixed-map `CMapFile`

Numeric facts: vtable RVA `0x00C2DA98`, path offset `0x50`, wide-string
storage, stack object at `ebp - 0x4B8`. This candidate is rejected because it
has no evidenced global root and is explicitly destroyed before the wrapper
returns.

### B. Persistent game-settings wide string

Numeric facts: root-pointer RVA `0x00E968A0`, one dereference, path-like wide
string at offset `0x00`. This candidate is rejected because the pointed object
does not have the `CMapFile` vtable, and no reviewed instruction proves that
its string is the path passed into the fixed-map `CMapFile` loader.

### C. Selected fixed-map menu entry

Numeric facts: selected-index RVA `0x0109C1EC`, array RVA `0x00E97848`, indexed
pointer dereference, ANSI string at entry offset `0x00`. This is sufficient for
a bounded menu-only research candidate but is rejected as the approved loaded-
map chain because the list is destroyed when the menu is torn down and the
selection callback uses entry offset `0x18`, not the path at `0x00`.

### D. `CRandomMaps`-owned `CMapFile`

Numeric facts: owner object RVA `0x00DCDA70`, `CMapFile*` offset `0x10`,
`CMapFile` vtable RVA `0x00C2DA98`, path offset `0x50`. The chain is complete
but rejected because the owner RTTI is `CRandomMaps`, outside the fixed-map
scope.

## Accepted Chain Initializer

None. A numeric `MapObjectLayout` initializer is intentionally not emitted:
there is no row satisfying the approved evidence rules. Compiling an internal
reader from any mixture of candidates A-D would violate the fail-closed gate.

The menu and post-init samples therefore do **not** have one accepted shared
root, and no pair of separately evidenced fixed-map roots was found.
