# Phase 6C.1 descriptor-family path evidence

Date: 2026-07-16 (Asia/Shanghai)

## Scope and authority

This document corrects only the page-16 Roman descriptor family and records
the immutable evidence used by the read-only diagnostic. It supersedes the
page-16/page-18 Mission CD rows in the Phase 6C evidence; it does not rewrite
that historical audit.

The inputs remain exact `S4_Main.exe` version `2.50.1516.0`, SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`,
and `S4_MainR.pdb`, SHA-256
`702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`.
The preferred image base is `0x00400000`.

No display label, `identity.name`, save name, database value, or string
containing `RD` was used. Live acceptance still requires exact, confirmed,
same-MapInit-session `identity.relative`.

## Closed page-16 chain

The exact executable chain is:

1. page-16 public controls are constructed beginning at VMA `0x004961F0`;
2. handler VMA `0x00496310` normalizes control IDs by subtracting `1903` and
   bounds the result to `0..12`;
3. control `1903` dispatches event `0x1B60` with ordinal `0`, while control
   `1904` dispatches the same event with ordinal `1`;
4. consumer VMA `0x00525340` recognizes `0x1B60`, retains the event ordinal,
   and selects transition kind `5` at state VMA `0x00524350`;
5. the common formatter path at VMA `0x00524701` normalizes kind `5` to table
   index `0`, increments the zero-based ordinal, and formats the exact literal
   at VMA `0x010573E0`;
6. the literal is `Map\Campaign\md_roman%01i.map`, producing only
   `Map\Campaign\md_roman1.map` and `Map\Campaign\md_roman2.map` for the two
   admitted public controls.

The previously associated `mcd2_*` initializer is a different formatter
family. The authoritative Phase 6C session independently confirmed
`Map\Campaign\md_roman1.map`; no string substitution was used to reach this
result.

## Frozen windows

File offsets use the exact PE section mappings. SHA-256 is over only the listed
window.

| Purpose | RVA | File offset | Length | Bytes | SHA-256 |
| --- | ---: | ---: | ---: | --- | --- |
| page-16 constructor | `0x000961F6` | `0x000955F6` | 8 | `85c90f8408010000` | `a50f992f0ceb2f0d32c48e8b03d12c1ad4f0a6cebf2a63dd8984e7ec497888e0` |
| click normalization | `0x000963EB` | `0x000957EB` | 14 | `0fb7450c0591f8ffff83f80c0f87` | `0a35944eba17601512609bb6182d30b6b41417da218c719cecfc0bbcee97e0d7` |
| relocated jump-target table | `0x00096584` | `0x00095984` | 28 | `3e64490071644900a4644900d4644900046549000b6449003e654900` | `b06f77787354aa3b1b17750dead81dea71bfc7c75d04abae1044fc21b8fd7ea2` |
| control-to-target index table | `0x000965A0` | `0x000959A0` | 13 | `00010203040606060606060605` | `992edff129b5315de0378ad5dd817650543e21f55986011e0aed52c8faceb460` |
| control 1903 ordinal/event | `0x00096452` | `0x00095852` | 11 | `6a006a006a0068601b0000` | `f2768f179a05ba1b0b1018c0c1f1546ee6713a9bd41b8d9cc4df9ae32d66764a` |
| control 1904 ordinal/event | `0x00096485` | `0x00095885` | 11 | `6a006a006a0168601b0000` | `fc870cc00e4d476c708c7b291ce1e5123f1d59f2726ecdbc85f0c6242b11710a` |
| event `0x1B60` selection | `0x00125368` | `0x00124768` | 20 | `8b4d088b51048bc283e80b74302d551b00007414` | `421284dc1431e491cff3bf1f636a62e1b805773ca09dcae1751daf8532cd1496` |
| event ordinal read | `0x00125390` | `0x00124790` | 3 | `8b5108` | `fa33eac9388e7b4949e29d4db1a9d3a35cdb2348a2c69931178ab48856874655` |
| transition kind 5 | `0x00125398` | `0x00124798` | 6 | `c1e21083ca05` | `18d77d2f7382757b3128c5ccde0aec6f8829489f1c341180b6f749a140f810fb` |
| formatter kind index | `0x00124701` | `0x00123B01` | 9 | `8b460483e8058d0440` | `eaa5cb7ae88973f0c4025b99777602260659e861429680c0c44a1765c5a22be4` |
| formatter length-table pointer | `0x0012470D` | `0x00123B0D` | 4 | `2cc34901` | `be80496ef048963a0b7b25999d941e24ed65e111ad56cfc35895daca8fee9ae6` |
| formatter string-table pointer | `0x00124715` | `0x00123B15` | 4 | `18c34901` | `bbcb34d9d92d279486e4b1825ff09db7d5769e1929492fccc34f5725e30f3573` |
| formatter ordinal increment | `0x0012471D` | `0x00123B1D` | 6 | `8b4608405051` | `2f3db2c757f20369f8104c1590d48ce9478c5f1ea8e0cfa0d3ebc95a7303e928` |
| initializer length/zero shape | `0x00023ADC` | `0x00022EDC` | 4 | `6a1d33c0` | `9d3cc958d2c4ff4d25e52560843a2a61de30e1b242683e96ed5dc5c6b30247f1` |
| initializer literal pointer | `0x00023AEB` | `0x00022EEB` | 4 | `e0730501` | `7ae5a97cbb14d9cd9594e0885fb8a6acc01938fbd1726d4c13340ba9648b6e4d` |
| initializer destination pointer | `0x00023AF0` | `0x00022EF0` | 4 | `18c34901` | `bbcb34d9d92d279486e4b1825ff09db7d5769e1929492fccc34f5725e30f3573` |
| exact UTF-16 literal | `0x00C573E0` | `0x00C567E0` | 60 | `4d00610070005c00430061006d0070006100690067006e005c006d0064005f0072006f006d0061006e0025003000310069002e006d00610070000000` | `f00453a60b4307b61e94b84036aa06f6ff62e90f6e1df7db6c9831d9102e115a` |

The jump-target and pointer operands are PE relocations. Runtime admission
compares their relocated values to `loaded_base + target_rva`; the bytes and
hashes above are the immutable on-disk representation.

## Admission decision

Only these corrected records are admitted for this family:

| Key | Page/control/rectangle | Ordinal/kind | Exact relative |
| --- | --- | --- | --- |
| `md-roman-01` | `16 / 1903 / 237,148,175,30` | `0 / 0x05` | `Map\Campaign\md_roman1.map` |
| `md-roman-02` | `16 / 1904 / 237,195,175,30` | `1 / 0x05` | `Map\Campaign\md_roman2.map` |

Pages 17–19, including the former page-18 Mayan record, are disabled because
their own public-control-to-dispatch-to-slot chains were not re-audited here.
New World and New World 2 remain disabled. A plausible `mcd2_*` path, sibling
page, translated name, or display/save value cannot arm a session lease.
