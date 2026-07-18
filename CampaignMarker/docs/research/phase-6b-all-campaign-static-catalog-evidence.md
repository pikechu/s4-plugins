# Phase 6B all-campaign static catalog evidence

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The exact approved executable statically proves the campaign path-format
families and the PDB proves the owning campaign state families. It does not yet
prove a complete public control/rectangle/ordinal catalog for every campaign
page. Dark Tribe is the only complete catalog admitted by the current evidence.

Accordingly, Phase 6B may define one common descriptor and one efficient batch
acceptance protocol, but it must not populate or render non-Dark campaign
markers until each descriptor has its own exact control-dispatch evidence and
one same-session `identity.relative` anchor. Missing evidence disables only that
descriptor; it is never filled from visible text, a save name, or a neighboring
campaign.

This is offline evidence and design input only. It is not implementation,
deployment, marker, database-write, campaign-progress, or save-file authority.

## Frozen inputs

- `S4_Main.exe` SHA-256
  `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`;
- `S4_MainR.pdb` SHA-256
  `702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6`;
- vendored `S4ModApi.h` SHA-256
  `31069630faffea89d21359bdc001261fbf4eef3307b7ee900be4d44934a835d5`;
- Phase 6A.1 live log, 12686712 bytes, SHA-256
  `3450567156fd241417248fe4dd6c476d0ed8d66cbc6f88164cb974bd7d2c9d06`.

No live process, database, save file, or installed archive was opened or
changed for this evidence pass.

## Public pages and PDB ownership

The vendored public enum assigns the following campaign screens:

| Content group | Public pages | PDB state evidence |
| --- | --- | --- |
| Original campaigns | 20 `SINGLEPLAYER_CAMPAIGN`; 21 `SINGLEPLAYER_DARKTRIBE` | `CStateCampaign3X3`, `CStateCampaignDark` |
| Add-on | 3 selector; 11 Trojan; 12 Roman; 13 Mayan; 14 Viking; 15 Settle | `CStateAOCampaigns`, race states, `CStateAOCampaignsSettle`, `CStateAOCampaignBonus` |
| Mission CD | 4 selector; 16 Roman; 17 Viking; 18 Mayan; 19 Conflict | `CStateMD2Campaigns` |
| New World | 5 | `CStateMDCampaigns`, Roman, Mayan, Viking, and EcoConflict states |
| New World 2 | 6 | `CStateXMD3Campaigns` |

The PDB names state ownership, not a safe runtime catalog object. These symbols
do not authorize calling a private function or reading mutable game state.
Historical public callbacks confirm that all listed page numbers occur in the
approved build, but sparse callback values are observations rather than a
complete directory.

## Exact path-format initializer windows

All RVAs are relative to preferred image base `0x00400000`. Raw hashes cover
the exact approved executable bytes.

| Group | VMA / RVA | File offset | Bytes | SHA-256 |
| --- | --- | ---: | ---: | --- |
| Add-on | `0x423090` / `0x00023090` | `0x22490` | 415 | `9459c9d0fc6c027ec736eb9bf9b3c0ddf6642edc0e8957919406bc8f81812799` |
| Original | `0x4233F0` / `0x000233F0` | `0x227F0` | 384 | `96a78fc162b178d98a4733fc758b0cc1fb1e73ba75ce51bd26b5a4308a6bd978` |
| Mission CD | `0x423980` / `0x00023980` | `0x22D80` | 297 | `d37b56be5c1509afac177a56b17f9a9849b1ebf25cc696de848fb6d74a1c5c56` |
| New World | `0x423AB0` / `0x00023AB0` | `0x22EB0` | 415 | `63278d29a609391708defde9fbbf28fd9c09d9f8a1ac9521fd8beab849aef8b8` |
| New World 2 | `0x423CA0` / `0x00023CA0` | `0x230A0` | 297 | `3c561626b14678bf2a94d76097c44b785a91dd520a0e6d4438cecb6e1576ce88` |

The windows initialize these exact UTF-16 formats:

| Group | Exact relative path formats |
| --- | --- |
| Add-on | `ao_roman%01i`, `ao_viking%01i`, `ao_maya%01i`, `ao_settle%01i`, `ao_trojan%02i`, `ao_bonus` |
| Original | `roman%02i`, `viking%02i`, `maya%02i`, `dark%02i`, `troja%02i` |
| Mission CD | `mcd2_roman%01i`, `mcd2_viking%01i`, `mcd2_maya%01i`, `mcd2_trojan%01i` |
| New World | `md_roman%01i`, `md_viking%01i`, `md_maya%01i`, `md_settle%01i`, `md_conflict%01i`, `md_bonus` |
| New World 2 | `xmd3_roman%01i`, `xmd3_viking%01i`, `xmd3_maya%01i`, `xmd3_trojan%01i` |

Each table cell is under `Map\Campaign\` and ends in `.map`; the compact table
omits only that common prefix and suffix. Spelling such as `troja` is preserved
exactly and must not be normalized from a translated menu label.

## Evidence matrix

| Required fact | Dark Tribe | Other campaign descriptors |
| --- | --- | --- |
| Exact executable and immutable path initializer | GO | GO |
| Public page ownership | GO | GO |
| PDB state-family ownership | GO | GO |
| Complete control ID to ordinal dispatch | GO | EVIDENCE GAP |
| Complete logical rectangles | GO | EVIDENCE GAP |
| Same-session click to confirmed `identity.relative` | GO (`dark03`) | EVIDENCE GAP |
| Marker authorization | NO | NO |

Phase 6A.2 already proves Dark controls 2081 through 2092, their rectangles,
and `dark01` through `dark12`. That result is reusable and should not be
retested merely because other descriptors are added. A change to shared public
association code, compatibility admission, or rendering would be a reason to
rerun its focused regression anchor.

## Strict identity boundary

Static formats constrain which relative identifiers are possible; they do not
prove which visible control selected one. A descriptor is admitted only after
an exact immutable construction/dispatch window proves its control IDs,
ordinals, bounds, and formatter selection. The public callback independently
checks page and geometry. One unique public click may then be associated only
with the same MapInit session's confirmed SU `identity.relative`.

`identity.name`, translated labels, player save names, filenames containing
`RD`, and database display fields are never classification inputs. RD archive
classification continues to use only confirmed session-bound
`identity.relative`. No ambiguity may fall back to presentation text.

## Static conclusion

One implementation can safely host all campaign descriptors, but code reuse is
not evidence reuse. The shared mechanism is reusable; each exact descriptor is
independently gated. The next authorized development, if approved, should
collect the missing public catalogs in one deployment and one game launch,
then use offline dispatch analysis to minimize dynamic map launches.
