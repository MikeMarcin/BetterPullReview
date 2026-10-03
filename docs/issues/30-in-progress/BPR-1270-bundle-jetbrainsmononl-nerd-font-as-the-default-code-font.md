---
schema: "implement-rfc.card.v1"
id: "BPR-1270"
title: "Bundle JetBrainsMonoNL Nerd Font as the default code font on every platform"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision relayed by the coordinator, 2026-10-03","reservation":"docs/issues/_reservations/BPR-1270--BPR-1270--82d8793db29a.json"}
---

# BPR-1270: Bundle JetBrainsMonoNL Nerd Font as the default code font on every platform

## Owner decision

Windows should default to JetBrains Mono NL like the other platforms, and the
owner chose the Nerd Font variant: the family **JetBrainsMonoNL Nerd Font**,
which `conversation_cards_view.cpp` already names. Today
`default_typography_settings` (`src/adapters/qt/src/appearance_profile.cpp`)
takes the operating system's fixed font: Menlo, Consolas or DejaVu Sans Mono.
A machine that has not installed the family never sees it, and the cards'
excerpt silently falls back to whatever Qt substitutes.

## Scope

1. **Vendor the font.** JetBrainsMonoNL Nerd Font Regular, Bold, Italic and
   BoldItalic (the code view's four faces: plain, syntax bold, syntax italic,
   both), from the official Nerd Fonts release asset at a pinned version and
   SHA-256, through the repository's pinned-archive convention (CPM source
   cache, `BPR_DEPENDENCY_OFFLINE`). Its licences, the SIL OFL 1.1 of JetBrains
   Mono and the Nerd Fonts glyph licences and attributions, go into the
   installed third-party notices, the release-source inventory and the SPDX
   records the release tooling checks.
2. **Embed and load it.** The four faces are Qt resources of the Qt adapter.
   The composition registers them with `QFontDatabase::addApplicationFont`
   after `QApplication` exists. A fresh profile's code font is the family on
   all three platforms; if loading fails the default falls back to the
   operating system's fixed font.
3. **Never override a saved code font.** A stored appearance record wins.
4. **The cards' excerpt uses the configured code font** (the appearance
   profile's), not a family named in the view.
5. **Tests** for loading and fixed-pitch resolution, the fresh default, the
   preserved saved choice, the fallback and the licensing records; any moved
   rendered check re-baselined with its reason, never by loosening a threshold.

## Design decisions

- **What counts as "chosen".** The appearance store keeps one whole record
  (`appearance_settings_database.cpp`): any stored change, a theme or a Dif
  preference included, writes the code family with it, and nothing records
  whether the family was picked or merely inherited. The record cannot tell a
  deliberate Menlo from a defaulted one, so a stored record is treated as the
  reviewer's choice and is never rewritten. Only a profile with no record (a
  fresh installation) and an explicit "reset code font" take the new default.
  A schema flag that told the two apart would be a storage migration and is
  not part of this card.
- **Which variant.** The owner's family is the plain Nerd Font variant, not
  "Nerd Font Mono". Its `post` table says `isFixedPitch = 0` (the Mono
  variant's says 1), but every mapped glyph advances 600 units, the OS/2 PANOSE
  proportion is 9 (monospaced), and the one 333-unit advance is an unmapped
  glyph. The tests assert fixed-pitch resolution through Qt on each platform,
  so a platform whose font database reads the `post` flag literally shows up
  as a failure rather than as a silent fallback.

## Acceptance criteria

- AC1: The four faces are acquired from the pinned official release asset,
  digest-checked, offline-buildable from the cache, and embedded.
- AC2: The bundled family loads and resolves fixed-pitch.
- AC3: A fresh profile's code font is the bundled family on macOS, Windows and
  Linux.
- AC4: A saved code font is preserved.
- AC5: When the bundled family cannot be loaded, the default is the operating
  system's fixed font.
- AC6: The cards' excerpt uses the appearance profile's code font.
- AC7: Notices, release-source inventory and SPDX records include the font and
  its licences, and the licensing tests pass.
- AC8: The round-trip composition pins are resealed with notes; the bundle size
  impact is recorded.
