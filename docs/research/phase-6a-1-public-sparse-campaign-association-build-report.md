# Phase 6A.1 public sparse campaign association build report

Date: 2026-07-16 (Asia/Shanghai)

## Decision

Phase 6A.1 is **GO for the public click-to-session association subproblem**.
The audited diagnostic was deployed and a user-driven live run proved the
ordered unique click, campaign MapInit binding, and exact same-session SU
relative association while the database remained byte-identical.

This decision does not admit an internal menu reader, campaign marker
rendering, victory handling, persistence, database repair, or any use of a
display/save name as map identity.

## Implemented boundary

The page-21 public GUI stream is retained as a bounded page-residency cache.
Empty callback intervals do not erase it. An exact duplicate is ignored and
only `effects` may update for an existing control key; any other structural
disagreement, overflow, or page-admission disagreement fails closed.

A unique exact labeled control may arm a 30-second launch lease. The lease
survives an invalid click redraw and the page-21 to briefing transition until
MapInit. Another page-21 click, abandoned re-entry, timeout, ineligible origin,
wrong session, empty `identity.relative`, or shutdown clears it. Only the exact
same-session confirmed `identity.relative` can be emitted.

## GREEN checkpoint

Commit `beb2d9d88d49b17f7a640aa74d4c7c03d1a0e123` is the complete checkpoint.
Before push, the changed production and test translation units compiled for
Win32 with strict warnings, the source-only read-only policy passed, and
`git diff --check` passed.

Authoritative Windows Release workflow run `29478420685`, job `87556431317`,
completed successfully for that exact head. Archive integration, Win32
configure/build with `/W4 /WX`, binary and source policy, all mutation
fixtures, tests, package construction, PE32/i386 and export verification, and
artifact upload all passed.

## Audited candidate

The independently downloaded artifact is frozen as:

- artifact ID `8367516201`, `CampaignCompletionDebug-Win32`, 236232 bytes;
- GitHub digest and downloaded outer SHA-256
  `9eaac55a64aceb91bdb58a49984629d5c673775e671db2137c5861f7f2eb096e`;
- embedded `CampaignCompletionDebug.zip`, 236199 bytes, SHA-256
  `ebc49ae4db5ee234f3c1bea8b299f5c1bf58b55bc20a9fd7d9777466ce9e93b9`;
- `Plugins/CampaignCompletionDebug.asi`, 420864 bytes, SHA-256
  `75f63da915be89373902262d3ff34b635c4a177443c3bb8844852fa853f1f637`;
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`, 895 bytes,
  SHA-256
  `26667d7af68b4919c65232dcd90989c087d9886f3e80a049d2e53746eebca0ec`.

The inner package contains exactly those two entries. Independent inspection
identifies the ASI as PE32/i386 with the single
`CampaignCompletionDebugStop` export. The packaged INI equals the committed
INI after normalizing the Windows checkout's CRLF line endings.

## Guarded deployment

With both protected applications closed and the user's fresh exact approval,
the audited candidate replaced only the project ASI and installed its matching
INI. The deployment is frozen as:

- previous installed archive SHA-256
  `9df807ccbee1d42e70ade9a0b0cc1e8b32d0ff2f98fbce5bc3d4eb9c0f632699`;
- deployed archive, 1395614 bytes, SHA-256
  `56df7205f94f59a36457ac37bac6ddbc529d1a8d55ebaa08503c3ce060541dca`;
- embedded ASI SHA-256
  `75f63da915be89373902262d3ff34b635c4a177443c3bb8844852fa853f1f637`;
- installed INI SHA-256
  `26667d7af68b4919c65232dcd90989c087d9886f3e80a049d2e53746eebca0ec`;
- immutable original archive SHA-256
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`.

The installed archive contains the seven immutable original entries plus the
single project ASI. No authorized temporary sibling remained.

## Live association proof

The accepted process loaded version `0.7.1` and the exact audited ASI in the
structurally read-only diagnostic mode. On entering Dark Tribe without an
intentional mission hover, the sparse cache accumulated Mission 2 through
Mission 6 and retained them across empty callback intervals. The launch then
logged this ordered evidence:

```text
campaign-menu-click armed control=2083 rect=500,168,175,30
ui-pages active=21,37 primary=37
map-init-session=1;list=unknown
campaign-menu-association session-armed=1
su-map-name=dark03
su-map-relative=Map\Campaign\dark03.map
identity-association=confirmed
campaign-menu-association page=21 control=2083 rect=500,168,175,30 session=1 relative=Map\Campaign\dark03.map
```

The association was emitted exactly once and used only the confirmed
session-bound `identity.relative`. The display name was not used for
classification or mapping.

## Zero-write postflight

After the user normally closed the game and Settlers United, protected process
count was zero. The database preflight and postflight identities were:

| File | Bytes | Last write UTC | SHA-256 |
| --- | ---: | --- | --- |
| `completed_maps.json` | 1260 | `2026-07-16T05:41:41.3482563Z` | `49b81aaffddd0380c6cfa69f870ad911d9b82f0ba55a213f305ad7955d4ff26e` |
| `completed_maps.json.bak` | 951 | `2026-07-14T11:13:02.1756072Z` | `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f` |

The archive, INI, and database temporary siblings were absent. The frozen live
log is 12686712 bytes with SHA-256
`3450567156fd241417248fe4dd6c476d0ed8d66cbc6f88164cb974bd7d2c9d06`.

## Remaining boundary

This GO is limited to exact public control-to-relative association. The
initial no-hover callback did not establish a complete stable catalog of all
12 missions, so the full no-hover campaign catalog remains an evidence gap.
No campaign marker, internal menu adapter, victory path, persistence, database
repair, or new deployment is authorized by this report.
