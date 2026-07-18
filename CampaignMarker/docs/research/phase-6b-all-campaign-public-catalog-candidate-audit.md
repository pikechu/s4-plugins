# Phase 6B all-campaign public catalog candidate audit

Date: 2026-07-16 (Asia/Shanghai)

## Decision

The Phase 6B read-only calibration candidate is GREEN for deployment review.
It broadens the Phase 6A.1 public sparse catalog and session association from
Dark Tribe page 21 to the exact campaign page set
`{3,4,5,6,11,12,13,14,15,16,17,18,19,20,21}`. It does not render campaign
markers, admit completion records, write the database, inspect saves, call a
private menu function, or patch the process.

This audit does not authorize deployment. Exact replacement of the Settlers
United archive still requires both protected processes closed and a fresh
explicit approval naming the Phase 6B candidate.

## Source checkpoint

- implementation commit:
  `f7ea70f41f9cc9cd413b467030ae2903116b1e92`;
- focused CI-test correction:
  `b229f431d46ef279cbcd5eb0a41c06bb1ec6cbda`;
- candidate version: `0.8.0`;
- mode: `all-campaign-public-catalog-calibration`.

The first CI run `29482399019` compiled and passed archive integration, policy,
and all mutation fixtures, then failed because a policy test searched for the
literal `if (!IsCampaignCatalogPage(page))` while the production guard was the
equivalent composed expression
`if (!campaignPageActive || !IsCampaignCatalogPage(page))`. The approved
one-line correction relaxed only that source-text matcher. Production code was
unchanged by the correction.

## Authoritative Windows CI

- workflow run: `29483026574`;
- job: `87570909435`;
- head SHA: `b229f431d46ef279cbcd5eb0a41c06bb1ec6cbda`;
- result: success;
- artifact ID: `8369294863`;
- artifact service digest:
  `sha256:a4f11bba2094555c6565167213097adb20b7f40bfc30b6e3c1529f1030d453f4`.

All authoritative steps passed: SU archive integration, Win32 Release
configure/build, full policy verification, all forbidden-behavior mutation
fixtures, complete CTest suite, packaging, PE32/package verification, and
artifact upload.

## Downloaded artifact audit

The downloaded GitHub artifact wrapper contains exactly one project package:

- downloaded ZIP size: 236256 bytes;
- downloaded ZIP SHA-256:
  `4cb51909d7180204bb230abdfffc3460fbfeae01079e92c3703d7e7d9523bd7b`;
- `Plugins/CampaignCompletionDebug.asi`, 421376 bytes, SHA-256
  `3eeac2946731e368a66739bc7f382580fccc57f5d5985d472c2fe29973428c54`;
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`, 938 bytes,
  SHA-256
  `c4079872d6534f3afd74a056aaf88c464e2d3a6ba76181e6f944b1d5964768`.

The ASI is PE32/i386 and exports exactly one
`CampaignCompletionDebugStop`. Its embedded strings identify version `0.8.0`,
the all-campaign calibration mode, the Phase 6B read-only boundary, typed
catalog snapshots, and page/control/session/relative association records. The
packaged INI is content-identical to the source INI after normalizing its CI
CRLF line endings.

## Runtime contract

- public GUI and frame callbacks only;
- fixed-capacity pointer-free feature copies;
- one sparse cache bound to one exact admitted campaign page at a time;
- page-typed snapshots so one family cannot inherit another family's catalog;
- one unique labeled control may arm a 30-second lease;
- entering a different campaign page clears an unbound candidate;
- MapInit must be an eligible campaign session;
- association is single-use and accepts only the same session's nonempty
  confirmed `identity.relative`.

`identity.name`, display text, and save filenames are not read by the
association. The regression fixture uses display name `RD_PlayerSave` and
confirmed relative `Map\Campaign\md_roman1.map`; only the latter is emitted.
The RD archive-name rule therefore remains unchanged: classification is based
only on confirmed session-bound `identity.relative`.

## Local checks

- core capture/association tests compiled as strict 32-bit Win32 and executed
  successfully;
- the runtime policy test compiled and executed successfully;
- modified runtime translation units passed strict 32-bit compilation;
- `verify_no_process_patch.ps1 -SourceOnly` passed;
- `git diff --check` passed.

The local host could not locate Visual Studio `dumpbin`, so the local full
binary verifier stopped at that availability check. The authoritative Windows
CI ran the same full verifier with `dumpbin` and passed, including PE32 and
export validation.

## Deployment boundary

The audited candidate is suitable only for the designed one-launch sequential
catalog calibration. It is not a campaign-marker candidate. Deployment must
not occur until the user confirms both the game and Settlers United are closed
and explicitly approves deployment of this exact Phase 6B candidate.
