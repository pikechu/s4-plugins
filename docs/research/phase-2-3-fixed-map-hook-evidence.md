# Phase 2.3 Fixed-Map Hook Evidence

## Decision

**ACCEPTED FOR SINGLE CALL-SITE HOOK**

The approved executable contains one fixed-map-only direct `CALL` whose five
bytes, target, three stack arguments, `this` register, callee cleanup, and path
object layout are all demonstrated by bounded instruction sequences. S4ModApi
2.3.2 exports the strict `hlib::CallPatch` constructor and patch lifecycle used
by the approved design.

This decision authorizes only the single call-site replacement at RVA
`0x000FEFA5`. It does not authorize a function-entry Detour, second Hook,
trampoline, heap scan, unrelated internal call, game-data write, or disk-file
mutation.

## Reproducibility and Input Identity

`research/scripts/collect_fixed_map_hook_evidence.sh` reads the installed files
without modifying them and refuses any identity mismatch.

| Input | SHA-256 |
| --- | --- |
| `S4_Main.exe` | `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816` |
| `S4_MainR.pdb` | `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6` |
| Installed `S4ModApi.dll` | `ed111491b1a6bd2822ccd21fec31adedfb35b6bf311cd568d9ff0e6e2193126f` |
| Pinned `S4ModApi.h` | `31069630faffea89d21359bdc001261fbf4eef3307b7ee900be4d44934a835d5` |
| Pinned `S4ModApi.lib` | `96025df625b7b718737b261278459e28932e7b107cc5c90cd2ecf32df1577a62` |

Two consecutive collector runs produced an empty diff after removing only
`generated_utc` and `workspace_commit`. The complete normalized output is
`research/evidence/fixed-map-hook-site.txt`.

The executable image base is `0x00400000`. Its `.text` section has VMA range
`0x00401000..0x00F14EF0`, so the call site, original target, and reviewed helper
instructions all lie in executable code. The call-site file offset is
`0x000FE3A5`.

## Reviewed Instruction Evidence

| Fact | Instruction VMA / RVA | Bytes | Decoded operand | Accepted invariant |
| --- | --- | --- | --- | --- |
| third argument | `0x004FEF97` / `0x000FEF97` | `6a 01` | `push 1` | third stack argument is numeric `1` |
| second argument | `0x004FEF99` / `0x000FEF99` | `6a 02` | `push 2` | second stack argument is numeric `2` |
| first argument address | `0x004FEF9B` / `0x000FEF9B` | `8d 45 d4` | `eax = ebp - 0x2C` | path object is a live stack object |
| first argument | `0x004FEF9E` / `0x000FEF9E` | `50` | `push eax` | first stack argument is the path object pointer |
| `this` object | `0x004FEF9F` / `0x000FEF9F` | `8d 8d 48 fb ff ff` | `ecx = ebp - 0x4B8` | `ecx` holds the stack-local `CMapFile` |
| approved patch site | `0x004FEFA5` / `0x000FEFA5` | `e8 66 23 fb ff` | direct call with signed rel32 `-318618` | exact five bytes must be live before patching |
| decoded target | call next VMA `0x004FEFAA` | rel32 `0xFFFB2366` | `0x004FEFAA - 318618 = 0x004B1310` | original target RVA is `0x000B1310` |
| callee `this` | `0x004B1315` / `0x000B1315` | `8b f1` | `esi = ecx` | target uses MSVC `thiscall` object register |
| first parameter | `0x004B1318` / `0x000B1318` | `8b 7d 08` | `edi = [ebp + 0x08]` | first stack parameter is the path object |
| second parameter | `0x004B132C` / `0x000B132C` | `8b 45 0c` | `eax = [ebp + 0x0C]` | second stack parameter is the numeric mode |
| third parameter | `0x004B13AA` / `0x000B13AA` | `80 7d 10 00` | compare byte `[ebp + 0x10]` with zero | third stack parameter is a Boolean-like value |
| callee cleanup | `0x004B141F` / `0x000B141F` | `c2 0c 00` | `ret 0x0C` | callee removes exactly three 32-bit stack parameters |
| source length | `0x0045815E` / `0x0005815E` | `8b 43 10` | `eax = [ebx + 0x10]` | x86 MSVC wide-string length is at `+0x10` |
| destination capacity | `0x00458186` / `0x00058186` | `83 7e 14 08` | compare `[esi + 0x14]` with `8` | wide-string capacity is at `+0x14`; SSO threshold is `8` |
| heap storage | `0x0045818C` / `0x0005818C` | `8b 16` | `edx = [esi]` | capacity at least `8` selects pointer storage at `+0x00` |
| inline storage | `0x00458190` / `0x00058190` | `8b d6` | `edx = esi` | capacity below `8` selects inline storage at `+0x00` |
| UTF-16 terminator | `0x00458198` / `0x00058198` | `66 89 3c 42` | write word zero at `storage + length*2` | elements are 16-bit and terminated at the validated length |
| source storage branch | `0x004581CD` / `0x000581CD` | `83 7b 14 08` | compare source capacity with `8` | the source path uses the same storage discriminator |
| source heap pointer | `0x004581D3` / `0x000581D3` | `8b 1b` | `ebx = [ebx]` | source pointer is dereferenced only for non-SSO storage |

Because x86 `__fastcall` places its first two declared parameters in `ecx` and
`edx`, an adapter declared with `mapFile` followed by an ignored `edx` parameter
and then the three original arguments has the same three stack arguments and
must emit `ret 0x0C`. The implementation plan separately requires inspection
of the generated adapter machine code; this static inference alone cannot waive
that CI gate.

## S4ModApi Patch Evidence

The pinned header declares `hlib::AbstractPatch::patch`, `unpatch`, `update`,
and `isPatched`, plus the strict `hlib::CallPatch` constructor accepting an
expected `Patch::BYTE5` and NOP count. The installed DLL exports the matching
x86 decorated symbols:

```text
??0CallPatch@hlib@@QAE@_KKPBUBYTE5@Patch@1@I@Z
?isPatched@AbstractPatch@hlib@@QBE_NXZ
?patch@AbstractPatch@hlib@@QAE_NXZ
?unpatch@AbstractPatch@hlib@@QAE_NXZ
?update@AbstractPatch@hlib@@QAE_NXZ
```

The production backend must still independently classify the live bytes before
installation and restoration. An unknown patch at the site is a conflict, not
an invitation to chain or overwrite it.

## Approved Numeric Layout

The only production layout admitted by this evidence is:

```cpp
inline constexpr FixedMapHookSiteSpec kApprovedFixedMapHookSite{
    0x000FEFA5u,
    0x000B1310u,
    {0xE8u, 0x66u, 0x23u, 0xFBu, 0xFFu},
    0x0000000Cu,
};
```

The expected original relative call remains valid when the image is relocated
because both the call site and target move by the same module-base delta.

## Failure Boundary

Runtime admission must recheck the executable version/hash, `.text` ownership,
checked address arithmetic, exact live five bytes, and decoded target. Failure
of any check disables this Hook while retaining public diagnostics and
controlled stop. No value in this report authorizes a different executable,
different call site, or guessed string layout.
