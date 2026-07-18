# Phase 5B Read-Only Fixed-Map Menu Adapter Report

Date: 2026-07-15 (Asia/Shanghai)

## Result

GO for the read-only internal menu adapter diagnostic. Rendering integration
is not part of this GO and remains separately gated.

The accepted implementation is commit `7d9950a01438fdb79ebbc8697dc3b94112338662`.
Windows CI run `29409145735`, job `87331786102`, passed Win32 compilation,
policy and mutation gates, all tests, PE32/package verification, and artifact
upload. The audited ASI SHA-256 is
`b09158c82af3b81c9c121b67df005ab83a024f25db5024cc91db08c9a3c24506`.

## Fail-closed correction

The first deployed diagnostic commit, `8bcf259`, rejected its own admission
gate and performed no internal menu reads. Static disassembly had supplied
`.text` section offsets for two instruction windows rather than image RVAs.
The values were short by the `.text` section RVA `0x1000`.

The corrected and live-confirmed image RVAs are:

- visible-row lookup: `0x0008d660`;
- scroll-down bound: `0x0008e700`;
- list construction: `0x001202ae`.

An elevated read-only sampler used only `OpenProcess(PROCESS_VM_READ)` and
`ReadProcessMemory`. It confirmed all three loaded instruction windows and
their ASLR-adjusted absolute operands. No process-memory write, protection
change, hook, cursor movement, or synthetic input occurred.

## Live evidence

The corrected adapter logged `internal-menu-adapter=admitted-read-only` for
the exact approved `S4_Main.exe`. The user performed all navigation and exited
the game normally.

Without hovering any map row, the first Single Player Maps snapshot was:

- count `44`, scroll base `0`;
- slot 0 `Map\Singleplayer\Aeneas.map`;
- slots 1..5 `AllGreen`, `Assimilate`, `BadSurprise`, `Bahlum-Kuk`, and
  `Camelot` under the same relative-path prefix.

One downward wheel step produced scroll base `1`, shifted the prior rows by
one slot, and introduced `Map\Singleplayer\Chief of the Thieves.map` in slot
5. One upward step restored the exact initial six-row snapshot.

The zero-hover tab snapshots were:

- Multiplayer Maps: count `42`, prefix `Map\Multiplayer\`, first row
  `4 Isles.map`;
- Custom Maps: count `156`, prefix `Map\User\`, with
  `Map\User\Antares.map` in slot 5.

After returning to the main menu, the page set became `{1}` and no further
`internal-menu` record was emitted. No `HeaderUnreadable`, `AliasMismatch`,
`CountOutOfRange`, `ScrollOutOfRange`, `EntryUnreadable`, `LabelInvalid`, or
`ConcurrentMutation` status occurred. Both `S4_Main` and Settlers United
closed normally, with no archive temporary sibling or stop-request file left.

## Field-semantics correction

Live evidence proves that the accepted MSVC x86 `std::wstring` at entry
`+0x18` contains the map-relative identifier, not merely the visible display
label. This is stronger matching evidence because it has the same form as the
validated completion record's `identity.relative` field.

The adapter still does not create or classify session identity. A future
rendering integration may use the menu-relative identifier only to look up an
already validated completion record and choose a visible row slot. Random-map
classification remains exclusively based on confirmed session
`identity.relative`; a player-chosen save/display name such as
`RD_PlayerSave` remains irrelevant.

## Deployment evidence

The corrected candidate was installed through the guarded Settlers United
archive procedure:

- immutable original archive SHA-256:
  `807e58bc92e20afbda4a99d7abdfcd05b87eb230fbb630e4330b487b6ba8c265`;
- installed archive SHA-256:
  `d76fcf90bbecb783af70d8051e3e38d07dfc92f25997910bd301e96930c14bf8`;
- installed INI SHA-256:
  `8416f2b08fa41acfc321d35e4c738b1d89e5825f4282e1dd30c0bba9932adbaa`;
- main database unchanged:
  `31edf4f486d7e0078efa23d958482ebc23ffadda2b555c73b5f49b2493756b1f`;
- backup database unchanged:
  `b372009a13739c9eafea5841c71eba8bbe91cac81e4e2a7e7b478191adfccc54`.
