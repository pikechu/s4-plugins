# Phase 6A Dark Tribe campaign-menu forensics report

Date: 2026-07-16 (Asia/Shanghai)

## Decision

Phase 6A is **EVIDENCE GAP**, not GO.

The audited structurally read-only candidate safely observed the Dark Tribe
page, one user-selected public control, MapInit, and the exact same-session
Settlers United relative identity. It did not produce the required stable
no-hover menu snapshot or its own click-to-identity association. The adjacent
observations must not be manually promoted into a mapping catalog.

This result authorizes no campaign marker, mapping entry, database change,
restoration, or additional deployment.

## Audited and deployed candidate

- source head: `f02805c704612b55aac81de61582a7a152a2a9aa`;
- Windows CI run `29476757736`: success;
- artifact ID `8366884532`;
- artifact SHA-256
  `1979e4168d37af6ccfe8c110c9df2779cd27ac7002e381bb9fd62e463045fbb8`;
- embedded package SHA-256
  `3904bc322e18410f8ab640bf6b2f7e9dd04d5c8860431e5c1fad7c64af688838`;
- ASI SHA-256
  `ddae1357af8f6fd3bb0eaee0c314baff24b95281080da52f1da777820aded617`;
- installed archive SHA-256
  `9df807ccbee1d42e70ade9a0b0cc1e8b32d0ff2f98fbce5bc3d4eb9c0f632699`;
- installed INI SHA-256
  `00facab7bdf7b70f6f6f20f72ece60dd381e1ffb4e0fdd57ab820a081d2bb5e7`.

The archive replacement changed only the project ASI. Every other entry
remained byte-identical to the immutable original.

## Read-only startup

The accepted process loaded the exact ASI and approved executable and logged:

```text
CampaignCompletionDebug bootstrap version=0.7.0 pid=40140 mode=dark-tribe-campaign-menu-forensics
module name=CampaignCompletionDebug.asi ... sha256=ddae1357af8f6fd3bb0eaee0c314baff24b95281080da52f1da777820aded617
executable compatibility=compatible
phase-6a-read-only storage=disabled native-events=disabled markers=disabled internal-menu-adapter=disabled
diagnostic runtime started
```

No completion store, completion commit, native victory subscriber, internal
menu reader, or marker renderer was started in this session.

## Public callback behavior

Page 21 was continuously and correctly identified. On first entry without
hover, the callback stream briefly emitted one feature:

```text
campaign-menu-snapshot page=21 generation=7 status=success count=1 feature=0:2083,500,168,175,30,...,effects=0,...,text=Mission 3
campaign-menu-snapshot page=21 generation=8 status=empty count=0
```

Hover reproduced the exact control key and changed only `effects` from `0` to
`2`, followed immediately by another empty snapshot. Moving away returned
`effects` to `0`, again followed by empty. Therefore the public GUI callback is
a sparse state-change stream on this page, not a complete per-frame menu
enumeration.

At launch, the successful hover snapshot was cleared about 30 ms later. The
left-button release then observed the exact control but could not arm the
association because the latest snapshot was empty:

```text
campaign-menu-snapshot page=21 generation=6573 status=success count=1 feature=0:2083,500,168,175,30,...,effects=2,...,text=Mission 3
campaign-menu-snapshot page=21 generation=6574 status=empty count=0
mouse page=21 button=1 message=513 ... element-id=2083 rect=500,168,175,30
campaign-menu-snapshot page=21 generation=6577 status=invalid count=0
campaign-menu-snapshot page=21 generation=6578 status=empty count=0
```

The click-time invalid snapshot shows that the same logical control can emit
volatile state variants during one capture epoch. Treating an `effects` change
as a conflicting duplicate is too strict for this sparse callback contract.

On return to page 21 without hover, no mission feature reproduced. A Back
control appeared only when the user left the page, also followed by empty and
invalid snapshots. Thus a no-hover stable public snapshot is not available
from the current frame-epoch model.

## Session identity evidence

The subsequent launch independently produced:

```text
map-init-session=1;list=unknown
su-map-name=dark03
su-map-relative=Map\Campaign\dark03.map
identity-association=confirmed
```

This proves that SU supplied `Map\Campaign\dark03.map` for session 1. It does
not prove the approved control mapping because the diagnostic did not log
`campaign-menu-click armed` or `campaign-menu-association`. Display text and
the `dark03` name are not substitutes for the missing association. The RD
archive-name rule remains unchanged: only confirmed session-bound
`identity.relative` may classify a map.

## Zero-write postflight

After automatic AI Resign, normal return, and user-controlled exit, protected
process count was zero. Preflight and postflight values were identical:

| File | Bytes | Last write UTC | SHA-256 |
| --- | ---: | --- | --- |
| `completed_maps.json` | 1260 | `2026-07-16T05:41:41.3482563Z` | `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| `completed_maps.json.bak` | 951 | `2026-07-14T11:13:02.1756072Z` | `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

The database, INI, and archive temporary siblings were absent. The frozen live
log is `12617019` bytes with SHA-256
`a933666577827684a1ef2cda7127c2bd4ca5d19326e064efc1fae18db158cd78`.

## Smallest next read-only investigation

The next candidate must remain structurally read-only and split the two gaps:

1. Replace per-frame replacement semantics with a bounded page-residency cache
   keyed by public control ID and rectangle. Empty callback intervals must not
   erase known features; volatile `effects` transitions may update a feature,
   while structural field disagreement still fails closed.
2. Arm only a uniquely cached mission-like control synchronously on its public
   left-button release. Preserve that bounded candidate across the observed
   page-21 to briefing-page-37 transition until MapInit, with the existing
   30-second lease. Explicit Back/navigation and shutdown must clear it.
3. Repeat the same-session SU identity test. This may close the exact
   control-to-relative association gap without any private reader.
4. Separately investigate the smallest approved read-only campaign-menu state
   probe for the no-hover full-menu catalog. Public callbacks alone did not
   supply it, so immediate marker design remains blocked even if step 3 later
   succeeds.

No internal address or layout is admitted by this report. Any private
campaign-menu adapter requires a new static-evidence design and explicit user
approval before development or deployment.
