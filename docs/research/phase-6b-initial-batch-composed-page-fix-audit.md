# Phase 6B initial batch and composed-page fix audit

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The first all-campaign catalog pass accepted the public sparse capture and
page isolation for pages 11 through 21, with zero invalid snapshots. It exposed
an evidence gap on selectors 3/4 and the shared New World page set 5/6: those
screens draw companion public layers, and candidate `0.8.0` cleared its single
page cache when a non-campaign companion frame arrived.

The composed-page owner fix plus the bounded startup retry are live-validated
in read-only candidate `0.8.2`. The focused selector retest produced successful
snapshots for pages 3, 4, and 6, with zero invalid snapshots, clicks, launches,
or identity associations. Normal-shutdown postflight preserved the database and
backup byte-for-byte. Phase 6B is therefore GREEN for the public catalog and
page-isolation calibration scope; it does not authorize inferred mission
mappings or campaign markers.

## Initial live slice

- installed candidate: `0.8.0`, audited ASI SHA-256
  `3eeac2946731e368a66739bc7f382580fccc57f5d5985d472c2fe29973428c54`;
- frozen log range: offsets `[12686712,12741849)`;
- range length: 55137 bytes;
- range SHA-256:
  `60d2c47a785c91f237d9c080458d6f6f86df7eaed3bee47175ab70ba66acfb93`;
- exact executable compatibility: accepted;
- runtime boundary: storage, native events, markers, and internal menu adapter
  disabled.

The batch emitted no campaign click or identity association because the user
correctly performed only the catalog pass.

| Page | Successful snapshots | Maximum retained features | Invalid |
| ---: | ---: | ---: | ---: |
| 11 | 6 | 2 | 0 |
| 12 | 2 | 1 | 0 |
| 13 | 3 | 2 | 0 |
| 14 | 2 | 1 | 0 |
| 15 | 2 | 1 | 0 |
| 16 | 5 | 3 | 0 |
| 17 | 2 | 1 | 0 |
| 18 | 3 | 2 | 0 |
| 19 | 3 | 1 | 0 |
| 20 | 4 | 3 | 0 |
| 21 | 4 | 4 | 0 |

Observed mission controls include Add-on Trojan `91`, Mayan `34`, Mission CD
Roman `1903/1904`, Mission CD Mayan `1889`, original `2039/2040`, and Dark
Tribe `2083/2084/2085`. Sparse absence on other entries remains absence, not an
inferred mapping.

Selectors 3/4 and pages 5/6 were visibly active but emitted no catalog
snapshot. Page histories proved the composition:

- Add-on selector: page set includes `3,24`;
- Mission CD selector: page set includes `4,22,23,25`;
- New World screens: page set stabilizes as `5,6`.

## Focused fix

Commit `de76e04e190f4cdc7296dd4fde8c39e04ec33f6b` introduces one immutable sorted
campaign page list and selects the highest active admitted page as the stable
catalog owner. Consequences:

- page 3 survives companion page 24;
- page 4 survives companion pages 22/23/25;
- the simultaneous 5/6 set uses deterministic owner 6;
- child pages 11 through 21 outrank their selector during transitions;
- leaving all admitted campaign pages still clears the cache;
- changing owner still isolates snapshots and clears an unbound cross-page
  click candidate.

The 5/6 owner is an observation key, not map identity. New World versus New
World 2 must still be distinguished by a later same-session confirmed
`identity.relative`, never by label or save name.

## Authoritative CI and artifact

- workflow run: `29484738722`;
- job: `87576414825`;
- result: success;
- artifact ID: `8369955051`;
- service digest:
  `sha256:4680b8967918f0b64b9c8c8e3da757eb712c4736b293a21bdffcb30910a3a508`;
- downloaded ZIP SHA-256:
  `38b7990299d5163541b14ee2b566db94ef5a3eaed54da41b0d4bcda648992f0f`;
- ASI: 421888 bytes, SHA-256
  `19293da03095ae9f4179778509a39de5291060afc92578caf49500f8186e9886`;
- INI: 938 bytes, SHA-256
  `bdab7dc5a685ad04c45e1af48c545b18a5f7a0a8bda81f88306c57b2e651af95`.

All archive integration, Win32 Release build, policy, mutation, CTest,
packaging, PE32, export, and package checks passed. The ASI has exactly one
`CampaignCompletionDebugStop` export, and the packaged INI is source-identical
after CRLF normalization.

## Startup retry follow-up

The first `0.8.1` composed-page retest was not evidence-bearing. The installed
ASI identity was exact, but the active game process recorded an empty module
inventory at bootstrap and failed closed with
`S4_Main.exe absent from module inventory`. It emitted no page snapshot, click,
session, or identity association during the user's four-page pass. The user's
navigation was correct and no catalog claim is made from that run.

Commit `3079a9d621dc3ba41c0a8139d7605cf070918b9d` adds a bounded read-only retry:
up to 20 module inventories separated by 100ms. It does not admit an empty
inventory and does not change the exact executable version/SHA gate. If the
approved executable is still absent, startup fails closed exactly as before.

The `0.8.2` retry candidate passed deployment review:

- workflow run `29485642641`, job `87579365098`, success;
- artifact ID `8370318400`;
- service digest
  `sha256:406cb6a92b9a7f86efa32949f56f0bdb0885fb381adb5c031732b8c79f5e52de`;
- downloaded ZIP SHA-256
  `9b832eac849502923f65abf6a3ada0388c46ced77cf92032bc17d9c39394a332`;
- ASI: 421376 bytes, SHA-256
  `097aac07991ed0f6324bfadec333afff8f16fe1ebd8a10cc6c72d660979666b1`;
- INI: 938 bytes, SHA-256
  `55c664baaf4f58d38c970686e36859eae6b58b718cdce1eca0b442a5ae45a1c2`.

The complete Windows build, policy, mutation, CTest, package, PE32, and export
gates passed.

## `0.8.2` deployment and focused live retest

The user closed both applications and explicitly approved deployment of the
Phase 6B startup-retry candidate. Exact replacement produced:

- installed archive SHA-256
  `3faaa2c537713e04b642d4adb15a495650ee0f5479c5558c939abe009dfcf936`;
- embedded ASI SHA-256
  `097aac07991ed0f6324bfadec333afff8f16fe1ebd8a10cc6c72d660979666b1`;
- installed INI SHA-256
  `55c664baaf4f58d38c970686e36859eae6b58b718cdce1eca0b442a5ae45a1c2`;
- preserved original archive SHA-256
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

The new session loaded exact candidate `0.8.2`, accepted exact executable
version `2.50.1516.0` and SHA-256
`3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`,
and started the diagnostic runtime. The focused no-launch batch observed the
composed active sets `3,24`, `4,22,23,25`, and `5,6`, while the stable catalog
owner emitted:

| Page | Successful snapshots | Maximum retained features | Invalid |
| ---: | ---: | ---: | ---: |
| 3 | 3 | 2 | 0 |
| 4 | 2 | 1 | 0 |
| 6 | 6 | 2 | 0 |

No campaign click, association, map session, or identity evidence was emitted.
The frozen `0.8.2` session occupies log offsets `[12792772,12833244)`, is 40472
bytes, and has SHA-256
`869580203b3422031a5681a86fd0a52fb9af9910c7b177f8eaac21e7b38c3967`.

After normal user-controlled shutdown, process inventory was empty and no
database or archive temporary file existed. Final state remained:

- `completed_maps.json`: 1260 bytes, SHA-256
  `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e`,
  timestamp `2026-07-16T05:41:41.3482563Z`;
- `completed_maps.json.bak`: 951 bytes, SHA-256
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`,
  timestamp `2026-07-14T11:13:02.1756072Z`;
- final log: 12833244 bytes, SHA-256
  `807de0a0e0f1cd819214ca914c02053114d6700033367b188805a3fe8a365942`.

## Scope conclusion

Pages 3, 4, 6, and 11 through 21 now share one proven bounded public sparse
catalog path with deterministic page isolation. Static path tables remain
independently gated. Any later RD classification must still use a confirmed
same-session `identity.relative`; save names, display labels, and page ownership
are not classification evidence. Dynamic mission anchors and campaign marker
implementation require their own approved phase.
