# Phase 7.4 container-offset marker candidate audit

Date: 2026-07-18

## Result

Phase 7.4 is **GREEN for the audited offline `0.13.4` candidate**.

The user-saved New World evidence showed that completed Mayan 1 and Mayan 2
were admitted correctly but drawn near the upper-left of the composite page.
The immutable descriptors and completion database identities were correct.
Read-only source and runtime evidence proved that the public GUI callback
reports mission coordinates relative to a containing element:

- `x/y` are the mission control's local coordinates
- `xOffset/yOffset` are the containing element's screen offsets

The prior renderer used only local `x/y`. The `0.13.4` candidate retains local
geometry for exact descriptor and same-session identity admission, but adds
the public container offset when producing bounded draw commands. Translated
geometry outside the surface fails closed and clears the frame.

This candidate supersedes the undeployed audited `0.13.3` candidate and also
retains its English-only manager UI, `Great Crusades` public label, and
pending-change Apply behavior.

## Source checkpoint

- commit: `b5f36cd2ddc97427c425a7a97922a06cab954487`
- version: `0.13.4`

## Evidence and boundary analysis

The user-saved `new-world-marker.png` is a 2194 by 1234 screenshot. It shows
two markers drawn at the composite page's local Mayan 1 and Mayan 2
coordinates rather than at their displayed controls.

The captured New World 2 descriptor controls remain:

- Mayan 1: control `1835`, local bounds `194,147,28,25`
- Mayan 2: control `1836`, local bounds `178,183,28,25`

The upstream public `CGuiElementBltHook` assigns `element->x/y` to callback
`x/y` and `container->x/y` to callback `xOffset/yOffset`. No internal menu
write, renderer, hook, patch, or database identity change is required.

## Local validation

- affected production translation units compiled as Win32 x86 with project
  warnings as errors: success
- affected test translation units compiled as Win32 x86: success
- the existing range-loop copy warning in `CampaignMenuCaptureTests.cpp` and
  the third-party S4ModApi ignored `uuid` attribute were non-product warnings
- source-only zero-process-patch and read-only-Lua verification: success
- capture test proves the public offsets are copied into owned storage
- observer test proves offsets are added only to output draw coordinates
- observer test proves translated out-of-surface geometry fails closed
- observer matching still succeeds with non-zero offsets, proving exact
  descriptor identity continues to use local geometry
- `git diff --check`: success

## Authoritative Windows CI

- workflow run: `29646186916`
- job: `88084724264`
- commit: `b5f36cd2ddc97427c425a7a97922a06cab954487`

All workflow steps succeeded:

- Settlers United archive integration
- Win32 Release configure and build
- complete PE/source policy verification
- all 11 forbidden-behavior mutation fixtures
- full CTest suite
- package
- PE32 and exact package-layout verification
- artifact upload

## Artifact audit

- artifact ID: `8430120493`
- GitHub artifact size: `314034`
- outer artifact SHA-256:
  `66ccad7d2e932ff12b1b3139789a9e5185385dde3c32586009a5c1857d542a1c`
- inner deployment ZIP SHA-256:
  `926bda2f4def9ea439115450926b30bb2c42b9b9617e9ce61e0f07bc458ff1f9`
- ASI SHA-256:
  `de10824a451bec3d566dbf417307c23fb9912fbf6aa2ea2b03e98e9d20655534`
- packaged INI SHA-256:
  `07f997418b2b86aed67b34db8915c1b52a7d2d64cdfb4d6b3c9928a33b567352`

The artifact contains exactly one inner deployment ZIP. That ZIP contains
exactly:

- `Plugins/CampaignCompletionDebug.asi`
- `Plugins/CampaignCompletion/CampaignCompletionDebug.ini`

The ASI is PE32 x86 and exports exactly
`CampaignCompletionDebugStop`. The packaged INI is source-equivalent after
line-ending normalization and declares `Version=0.13.4`.

Binary inspection found the English manager title, Refresh, Apply, Close, and
`Campaigns / Great Crusades`. The former Chinese manager strings and
`Campaigns / New World 2` are absent.

The local full policy script could not repeat its `dumpbin.exe` step because
that Visual Studio tool is unavailable in the current shell. The authoritative
Windows CI ran and passed the complete binary/export policy check; local
`file` and MinGW `objdump` independently confirmed PE32 x86 and the single
expected export.

## Guarded deployment

After both `S4_Main` and Settlers United were confirmed closed, the user gave
fresh explicit approval for the audited Phase 7.4 candidate and required
`PileChainRepair` to remain unchanged.

The fixed-hash elevated deployment:

- verified the current combined archive baseline SHA-256:
  `81de1082b1c3f17008708c9db8d0219330ea94db861d8508d4395af4ec9a77fd`
- verified the immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`
- created and verified a complete combined-archive rollback copy
- replaced only `Plugins/CampaignCompletionDebug.asi`
- installed archive SHA-256:
  `5378530b85874f7b3a4ae7b5b77b044753d7976634a8770a903a350ec712d64b`
- independently verified the embedded Campaign ASI SHA-256:
  `de10824a451bec3d566dbf417307c23fb9912fbf6aa2ea2b03e98e9d20655534`
- independently verified `Plugins/PileChainRepair.asi` remained:
  `7d17f6de446d43a22bfe30647585ba1bd2b5a4acc3781cb5b9eb24f4ead33493`
- independently compared all nine file entries and found only the Campaign
  ASI entry changed
- left no authorized temporary sibling
- confirmed both protected processes remained absent after deployment

The installed archive is 1,936,643 bytes. The guarded rollback baseline and
candidate remain in the project-owned Phase 7.4 backup directory.

RD handling remains fail-closed. Only exact same-session
`identity.relative` is admitted. An exact `RD_` relative remains random,
marker-hidden, non-editable, and is never displayed or named by the manager.
