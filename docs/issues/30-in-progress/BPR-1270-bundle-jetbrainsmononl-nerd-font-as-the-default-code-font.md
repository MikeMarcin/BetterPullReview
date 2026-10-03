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

## Implementation (a54728d13, macOS arm64)

- `cmake/BprCodeFontProvider.cmake` acquires
  `https://github.com/ryanoasis/nerd-fonts/releases/download/v3.5.1/JetBrainsMono.tar.xz`
  (7,284,244 bytes, SHA-256 `04d5e8f9…10cf`), extracts only the four faces,
  `OFL.txt` and `README.md`, checks each face's SHA-256 and that the licence
  and README equal the copies in `third_party/jetbrains-mono-nerd-font/`, and
  copies changed bytes only into `_bpr/code-font`. `BetterPullReview::CodeFont`
  carries the directory and file names; its audit record is
  `JetBrainsMonoNLNerdFont|PINNED_ARCHIVE|BetterPullReview::CodeFont`.
- `bpr_add_qt_resources` gains `ALWAYS_COMPRESS` (rcc threshold 0). rcc's
  default threshold stores a TTF whole; compressed, the faces go from
  9,967,348 to 5,456,414 bytes.
- `bpr::qt::register_bundled_code_font()` (`bundled_code_font.hpp`) adds the
  faces and removes a partial registration; `main.cpp` calls it after
  `QApplication`. `default_typography_settings()` takes the bundled family
  where `QFontDatabase::hasFamily` resolves it, at the platform fixed font's
  size and regular weight; `default_typography_settings(false)` is the
  fallback. The BPR_BUILD_TESTING `--code-font-probe` prints the fresh code
  font as the real startup resolves it.
- `conversation_cards_view::set_code_font` (from
  `github_conversation_panel::adopt_appearance`) replaces the named family;
  before a profile arrives the view uses the default typography's code font.
  The theme preview strip's unsupplied font is the same default.
- Licensing: `packaging/dependencies.json` component
  `JetBrainsMonoNL-Nerd-Font` (SPDX package, corresponding-source archive),
  `NOTICE_FILES` in `packaging/package_runtime.py` (now also used by
  `scripts/verify_release_security.py`), and the retained texts with their
  digests in `third_party/jetbrains-mono-nerd-font/PROVENANCE.md`.
- Also: 414490c9c rewords a BPR-1169 comment that tripped
  `core_dependency_boundary` at 4ee9e709a (the clock pattern matched "a page at
  a time (").

## Evidence (macOS 26 arm64, Qt 6.11.2, `build/dev` Debug)

- `qt_bundled_code_font` passes: refused registrations (no files, absent face,
  wrong family, partial) leave the database as found; the fallback is the
  platform fixed font; all four styles resolve to the family, bold/italic
  match, `QFontInfo::fixedPitch()` is true and every printable ASCII character
  advances one cell (a proportional face fails the same check); a fresh
  profile and the composed preferences host with no record use the family and
  write nothing; a stored record (the old platform default, and "Courier New")
  is kept and not rewritten; reset code font takes and stores the family; the
  conversation panel hands the cards the profile's code font.
- `application_code_font` passes through the real executable:
  `CODE_FONT_PROBE registered=yes default="JetBrainsMonoNL Nerd Font"
  resolved="JetBrainsMonoNL Nerd Font" fixed_pitch=yes` (offscreen and cocoa).
  Note this Mac also has the family installed in `~/Library/Fonts`, so on it
  only `registered=yes` proves the bundled faces load; the stations, which do
  not have it installed, are the meaningful resolution check.
- `qt_conversation_cards_view` adds the excerpt test: a larger code font lays
  the excerpt out taller at once, and an equal font measures nothing.
- All 118 `qt_*` rows pass. Of the 469 other rows, 465 passed in one parallel
  run; `coding_format_check`, `test_network_isolation_manifest` (the new row's
  own fail regex) and `core_dependency_boundary` were fixed and pass, and
  `application_profile_window_review_restoration_public_path` fails at "failed
  reading restoration preserves exact recorded identities and sibling", the
  known [[BPR-1236]] (fails the same way on Windows and Linux).
- No rendered pixel check moved on macOS, so nothing was re-baselined and no
  threshold changed. On Windows and Linux the default changes from Consolas or
  DejaVu Sans Mono, so their station runs are where a moved check would show.
- `package_runtime_contract` (17 tests, including the new font
  pin/notice/provenance test and a `notices()` run that finds every retained
  text whole; removing one entry from `NOTICE_FILES` fails it),
  `release_source_assets_contract`, `release_package_audit_contract`,
  `release_security_audit_contract`, `dependency_modernization_contract`,
  `architecture_boundaries`, `application_review_round_trip_composition_contract`
  (resealed) pass. `core-only` builds.
- Provider: offline with no cached archive refuses; offline with the cached
  archive succeeds; a tampered archive refuses; online acquisition from the
  release URL into an empty cache succeeds with the pinned digest.
- `verify_documentation.py` DOCUMENTATION_OK.

## Size impact

| Artifact | Change |
|---|---|
| Embedded faces (per architecture slice) | +5,463,600 bytes object (5,456,414 bytes rcc data; 9,967,348 raw) |
| macOS universal binary (two slices) | about +10.9 MB |
| Linux binary | about +5.5 MB |
| Windows `better_pull_review.rcc` | about +5.5 MB (from about 3.5 MB) |
| Corresponding-source asset | +7,284,244 bytes (the archive) |
| THIRD-PARTY-NOTICES.txt | about +45 KB |
| Startup | `--application-icon-probe`, which now includes registration, takes 0.10 s |

The universal binary carries the faces twice. Shipping them as files in
`Contents/Resources` would halve that on macOS; not done, as the decision was
"Qt resource or packaged fonts dir".

## Owed elsewhere

Linux station (Ubuntu 24.04, from a fresh pull of `main`):

```sh
cmake --preset dev && cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R '^(qt_bundled_code_font|application_code_font|qt_conversation_cards_view|qt_github_conversation_panel|qt_theme_preview_strip|qt_appearance_profile|qt_type_preview|package_runtime_contract|release_source_assets_contract|release_package_audit_contract|release_security_audit_contract|dependency_modernization_contract|architecture_boundaries|test_network_isolation_manifest|application_review_round_trip_composition_contract)$'
ctest --test-dir build/dev -R '^qt_' --output-on-failure
```

Windows station (`windows-lane` merged with `main`, native VS x64 tools
environment):

```powershell
cmake --preset dev; cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R '^(qt_bundled_code_font|application_code_font|application_installed_resources_contract|qt_conversation_cards_view|qt_github_conversation_panel|qt_theme_preview_strip|qt_appearance_profile|qt_type_preview|package_runtime_contract|release_source_assets_contract|release_package_audit_contract|release_security_audit_contract|dependency_modernization_contract|architecture_boundaries|test_network_isolation_manifest|application_review_round_trip_composition_contract)$'
ctest --test-dir build/dev -R '^qt_' --output-on-failure
```

`application_installed_resources_contract` (Windows only) now also requires
the four faces in the resource bundle, compressed and byte-equal to the
provider's verified faces. On both stations, `qt_bundled_code_font` asserting
`QFontInfo::fixedPitch()` is the check the `post` table's `isFixedPitch = 0`
could fail (DirectWrite or fontconfig reading the flag literally); a failure
there is a finding to report, not a threshold to relax. Any rendered row that
moves because the default code family changed is re-baselined on that station
with its reason. A packaging run (`linux-package`, `windows-app-release` with
`packaging/package_runtime.py deploy`) should show the component in
`sbom.spdx.json`, the texts in `ThirdParty/THIRD-PARTY-NOTICES.txt` and the
archive in the corresponding-source ZIP; that run is not a default and is owed
only at the next release.

## Open

- The Nerd Fonts repository's own MIT LICENSE text and the texts for Devicons,
  Hack extra glyphs, Font Awesome Extension, Seti UI, Power Symbols IEC and
  Font Logos are not in any v3.5.1 release asset, and downloading from
  elsewhere was not authorized. Their names, upstreams and licences are in the
  retained README table. Fetching the texts needs the owner's say-so.
- Linux and Windows station evidence (above).

## Checkpoint

Candidate a54728d13 (on 414490c9c). Next action: the Linux and Windows station
runs above; then finish the card with their evidence.
