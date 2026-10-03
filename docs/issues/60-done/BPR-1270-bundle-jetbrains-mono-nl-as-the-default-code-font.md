---
schema: "implement-rfc.card.v1"
id: "BPR-1270"
title: "Bundle JetBrains Mono NL as the default code font on every platform"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision relayed by the coordinator, 2026-10-03","amendment":"owner, 2026-10-03: plain JetBrains Mono NL instead of the Nerd Fonts build","reservation":"docs/issues/_reservations/BPR-1270--BPR-1270--82d8793db29a.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"JetBrains Mono 2.304 zip pinned by size and SHA-256 with per-face SHA-256; offline-with-cache builds, offline-without-cache and tampered archive refuse (327130672 provider scratch tests); fetched and verified on Linux (85f9ce78d) and Windows; faces embedded compressed."}},{"criterion":"AC2","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"application_code_font prints registered=yes resolved=\"JetBrains Mono NL\" fixed_pitch=yes on macOS (no plain family installed), Linux (no JetBrains font installed, 85f9ce78d) and Windows; qt_bundled_code_font passes on all three."}},{"criterion":"AC3","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"qt_bundled_code_font fresh-profile case passes on macOS, Linux and Windows; review-cycle courses register the bundled font as main.cpp does (6e220f1f3) and the share sheet resolves to JetBrains Mono NL on Windows."}},{"criterion":"AC4","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"qt_bundled_code_font saved-font case passes on all three platforms; a stored record keeps its font, reset stores the new default."}},{"criterion":"AC5","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"qt_bundled_code_font failed/partial-load and fallback cases pass on all three platforms."}},{"criterion":"AC6","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"conversation_cards_view and theme_preview_strip take the profile's code font; qt_conversation_cards_view and qt_github_conversation_panel pass on all three platforms."}},{"criterion":"AC7","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"packaging/dependencies.json lists JetBrains-Mono-NL 2.304 OFL-1.1; notices carry OFL.txt and AUTHORS.txt; package_runtime_contract, release_source_assets_contract, release_package_audit_contract, release_security_audit_contract and dependency_modernization_contract pass on macOS, Linux and Windows; Nerd Font entries removed and refused by test."}},{"criterion":"AC8","outcome":"pass","revision":"dcf3ac604","evidence":{"kind":"observation","detail":"Round-trip pin resealed with note at a54728d13; plain-font switch moved no pin (note recorded); application_review_round_trip_composition_contract passes on all three. Size: +413,284 bytes compressed per architecture (~0.83 MB macOS universal, ~0.41 MB Linux/Windows), +5,622,857 bytes corresponding source."}}]
---

# BPR-1270: Bundle JetBrains Mono NL as the default code font on every platform

## Owner decision

Windows should default to JetBrains Mono NL like the other platforms, and the
owner chose the Nerd Font variant: the family **JetBrainsMonoNL Nerd Font**,
which `conversation_cards_view.cpp` already names. Today
`default_typography_settings` (`src/adapters/qt/src/appearance_profile.cpp`)
takes the operating system's fixed font: Menlo, Consolas or DejaVu Sans Mono.
A machine that has not installed the family never sees it, and the cards'
excerpt silently falls back to whatever Qt substitutes.

## Owner amendment (2026-10-03)

After the first candidate (a54728d13, the Nerd Fonts build), the owner wrote:
"okay if its simpler you can use the non-nerd font". It is simpler, so the
bundled family is now plain **JetBrains Mono NL** from the official JetBrains
Mono release:

- **Licensing.** The plain font is SIL OFL 1.1 and nothing else. The Nerd
  Fonts build added nine glyph sets under MIT, CC BY 4.0, Apache 2.0 and OFL,
  which made its SPDX declaration `NOASSERTION` and left six licence texts that
  no release asset carried (recorded as open on the first candidate). Both
  problems go away.
- **Metadata.** The plain faces' `post` table says `isFixedPitch = 1`; the
  Nerd Font variant's said 0, which the first candidate's design note had to
  argue around.
- **Size.** 845 KB of faces instead of 9.97 MB.

The amendment replaces the family and source in scope item 1 and AC1/AC7; every
other item and criterion stands. The Nerd Fonts archive, its provenance and
licence texts, the FontPatcher.zip licence source and their notice entries were
removed entirely (327130672). Design files under `docs/design/` that list the
Nerd family in a CSS fallback stack are historical mockups and are unchanged.

## Scope

1. **Vendor the font.** JetBrains Mono NL Regular, Bold, Italic and BoldItalic
   (the code view's four faces: plain, syntax bold, syntax italic, both), from
   the official JetBrains Mono release asset at a pinned version and SHA-256,
   through the repository's pinned-archive convention (CPM source cache,
   `BPR_DEPENDENCY_OFFLINE`). Its licence, the SIL OFL 1.1, goes into the
   installed third-party notices, the release-source inventory and the SPDX
   records the release tooling checks. (Amended; first stated as the Nerd Fonts
   build with its glyph licences.)
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
  not part of this card. A profile that stored the first candidate's
  "JetBrainsMonoNL Nerd Font" keeps that family too; only developer profiles
  built from a54728d13 can hold it, and reset code font moves them.
- **Fixed pitch.** The tests still assert fixed-pitch resolution through Qt on
  each platform and that every printable ASCII character advances one cell, so
  a platform that substitutes or misreads the face fails rather than falls
  back silently.

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
  its licence, and the licensing tests pass.
- AC8: The round-trip composition pins are resealed with notes; the bundle size
  impact is recorded.

## Implementation (a54728d13, amended by 327130672; macOS arm64)

- `cmake/BprCodeFontProvider.cmake` acquires
  `https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip`
  (5,622,857 bytes, SHA-256 `6f6376c6…7bbf`; v2.304 is the latest release,
  2023-01-14), extracts only `fonts/ttf/JetBrainsMonoNL-{Regular,Bold,Italic,BoldItalic}.ttf`,
  `OFL.txt` and `AUTHORS.txt`, checks each face's SHA-256 and that both texts
  equal the copies in `third_party/jetbrains-mono/`, removes any staged file
  that is no longer a pinned face, and copies changed bytes only into
  `_bpr/code-font`. `BetterPullReview::CodeFont` carries the directory and file
  names; its audit record is
  `JetBrainsMonoNL|PINNED_ARCHIVE|BetterPullReview::CodeFont`.
- `bpr_add_qt_resources` has `ALWAYS_COMPRESS` (rcc threshold 0). rcc's
  default threshold stores a TTF whole; compressed, the faces go from 845,320
  to 413,284 bytes.
- `bpr::qt::register_bundled_code_font()` (`bundled_code_font.hpp`) adds the
  faces and removes a partial registration; the family is
  `"JetBrains Mono NL"`. `main.cpp` calls it after `QApplication`.
  `default_typography_settings()` takes the bundled family where
  `QFontDatabase::hasFamily` resolves it, at the platform fixed font's size and
  regular weight; `default_typography_settings(false)` is the fallback. The
  BPR_BUILD_TESTING `--code-font-probe` prints the fresh code font as the real
  startup resolves it.
- `conversation_cards_view::set_code_font` (from
  `github_conversation_panel::adopt_appearance`) replaces the named family;
  before a profile arrives the view uses the default typography's code font.
  The theme preview strip's unsupplied font is the same default.
- Licensing: `packaging/dependencies.json` component `JetBrains-Mono-NL`
  2.304, SPDX `OFL-1.1` (the SPDX package and the corresponding-source archive
  follow from it); `NOTICE_FILES` in `packaging/package_runtime.py` (also used
  by `scripts/verify_release_security.py`) carries `jetbrains-mono/OFL.txt`
  and `jetbrains-mono/AUTHORS.txt`; `third_party/jetbrains-mono/PROVENANCE.md`
  records the archive, the faces and the texts with their digests.
- Also: 414490c9c rewords a BPR-1169 comment that tripped
  `core_dependency_boundary` at 4ee9e709a (the clock pattern matched "a page at
  a time (").

## Evidence (macOS 26 arm64, Qt 6.11.2, `build/dev` Debug, at 327130672)

Plain JetBrains Mono NL is not installed on this Mac (`system_profiler
SPFontsDataType` lists only the Nerd families in `~/Library/Fonts`), so here,
unlike the first candidate's run, resolution is a real check of the bundled
faces.

- `qt_bundled_code_font` passes: refused registrations (no files, absent face,
  wrong family, partial) leave the database as found, and the family is not
  resolvable before registration; the fallback is the platform fixed font; all
  four styles resolve to the family, bold/italic match,
  `QFontInfo::fixedPitch()` is true and every printable ASCII character
  advances one cell (a proportional face fails the same check); a fresh
  profile and the composed preferences host with no record use the family and
  write nothing; a stored record (the old platform default, and "Courier New")
  is kept and not rewritten; reset code font takes and stores the family; the
  conversation panel hands the cards the profile's code font.
- `application_code_font` passes through the real executable:
  `CODE_FONT_PROBE registered=yes default="JetBrains Mono NL" resolved="JetBrains
  Mono NL" fixed_pitch=yes`, offscreen and cocoa.
- All 118 `qt_*` rows pass (serial run). No rendered pixel check moved, so
  nothing was re-baselined and no threshold changed.
- `package_runtime_contract` (17 tests: the pin/notice/provenance test now also
  checks version 2.304 and `OFL-1.1`, and refuses any Nerd Fonts entry in the
  manifest or `NOTICE_FILES` and a `third_party/jetbrains-mono-nerd-font`
  directory; the `notices()` run finds both retained texts whole),
  `release_source_assets_contract`, `release_package_audit_contract`,
  `release_security_audit_contract`, `dependency_modernization_contract`,
  `architecture_boundaries`, `test_network_isolation_manifest`,
  `core_dependency_boundary`, `coding_format_check`,
  `qt_conversation_cards_view` and
  `application_review_round_trip_composition_contract` pass. The round-trip
  pins are unchanged (main.cpp unchanged); its reseal note records the
  amendment.
- Provider, in a scratch project against scratch caches: offline with no cached
  archive refuses; offline with the cached archive succeeds; a tampered archive
  refuses; an edited retained `AUTHORS.txt` refuses; online acquisition from
  the release URL into an empty cache succeeds with the pinned digest. The
  `build/dev` reconfigure acquired the zip online and removed the four stale
  Nerd faces from the stage.
- `verify_documentation.py` DOCUMENTATION_OK.

The first candidate's macOS evidence (a54728d13, recorded in 67941cbea) is
superseded by the above. Its run had one unrelated failure,
`application_profile_window_review_restoration_public_path`, the known
[[BPR-1236]]; it was not rerun here.

## Size impact

| Artifact | Change |
|---|---|
| Embedded faces (per architecture slice) | +413,284 bytes rcc data (845,320 raw; 413,720-byte `__const` section) |
| macOS universal binary (two slices) | about +0.83 MB |
| Linux binary | about +0.41 MB |
| Windows `better_pull_review.rcc` | about +0.41 MB |
| Corresponding-source asset | +5,622,857 bytes (the release zip) |
| THIRD-PARTY-NOTICES.txt | about +5 KB (OFL and AUTHORS.txt) |
| Startup | `--application-icon-probe`, which includes registration, takes 0.09 s |

Against the Nerd Fonts candidate this is about 5.0 MB less per slice, 1.7 MB
less in the corresponding-source asset and about 40 KB less in the notices.

## Owed elsewhere

Linux station (Ubuntu 24.04, from a fresh pull of `main`; the configure
downloads `JetBrainsMono-2.304.zip` into the CPM cache):

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

`application_installed_resources_contract` (Windows only) requires exactly the
four `JetBrainsMonoNL-*.ttf` faces in the resource bundle, compressed and
byte-equal to the provider's verified faces. On both stations the default code
family changes from Consolas or DejaVu Sans Mono, so any rendered row that
moves is re-baselined there with its reason; a `qt_bundled_code_font` failure
is a finding to report, not a threshold to relax. A packaging run
(`linux-package`, `windows-app-release` with `packaging/package_runtime.py
deploy`) should show `JetBrains-Mono-NL` with `OFL-1.1` in `sbom.spdx.json`,
both texts in `ThirdParty/THIRD-PARTY-NOTICES.txt` and the zip in the
corresponding-source ZIP; that run is not a default and is owed only at the
next release.

## Open

- Linux and Windows station evidence (above).

## Checkpoint

Candidate 327130672 (on 67941cbea). Next action: the Linux and Windows station
runs above; then finish the card with their evidence.

## Linux: bpr-main-linux, 2026-10-03

At `dc25ffc4c` (includes `327130672`), on WSL2 with GCC 15.2, Qt 6.11.2 and
`build/dev` Debug, offscreen. This system has no JetBrains font
(`fc-list | grep -i jetbrains` finds none), so the bundled copy is what loads.

- `cmake --preset dev && cmake --build --preset dev` exits 0 with 0
  `warning:` lines. The pinned `JetBrainsMono-2.304.zip` was fetched into the
  dependency cache, and the cached Nerd Fonts archive from the withdrawn variant
  was deleted.
- **The bundled font loads and is fixed-pitch.** `application_code_font` prints
  `CODE_FONT_PROBE registered=yes default="JetBrains Mono NL"
  resolved="JetBrains Mono NL" fixed_pitch=yes`, and `qt_bundled_code_font`
  passes.
- The first requested selection passes **15 of 15** in 97.52 s, including
  `dependency_modernization_contract` (69.31 s), the release and package
  contracts, `test_network_isolation_manifest`, the round-trip contract and
  `architecture_boundaries`.
- All `^qt_` rows: **110 of 116** pass in 72.03 s. The 6 failures are
  [[BPR-1255]]'s `qt_workbench_title_row_scale_*` and
  `qt_workbench_window_scale_*`, each only at `no review-bar control folds into
  an overflow menu at any width`. No rendered row moved with the new default
  (from DejaVu Sans Mono), so nothing was re-baselined and no threshold was
  touched.

No remote mutation beyond the authorized font download.

## Windows (bpr-main-win, 2026-10-03)

At `dc25ffc4c`, which contains `327130672`: plain JetBrains Mono NL 2.304.
Windows 11 Home 10.0.26200, Visual Studio 18 (MSVC 14.51), Qt 6.11.2,
`build/windows-dev` Debug. The build fetched and verified the pinned JetBrains
release asset. The 54 executables these rows run were deleted first, and the
build rebuilt every one. It printed no `warning C` or `warning LNK` line.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^(qt_bundled_code_font|application_code_font|application_installed_resources_contract|qt_conversation_cards_view|qt_github_conversation_panel|qt_theme_preview_strip|qt_appearance_profile|qt_type_preview|package_runtime_contract|release_source_assets_contract|release_package_audit_contract|release_security_audit_contract|dependency_modernization_contract|architecture_boundaries|test_network_isolation_manifest|application_review_round_trip_composition_contract)$|^qt_"
```

120 of 127 pass, in 405.74 s, every named row among them:

- `qt_bundled_code_font` 0.28 s and `application_code_font` 0.17 s;
- `qt_appearance_profile`, `qt_type_preview`, `qt_theme_preview_strip`,
  `qt_conversation_cards_view` and `qt_github_conversation_panel`;
- the installed-resources, package, release-asset, audit and dependency
  contracts;
- the round-trip contract;
- `architecture_boundaries`;
- `test_network_isolation_manifest`.

The seven failures are parked rows, each with its earlier headline:

- `qt_agent_launch_canary`: `open durable child course` ([[BPR-1229]]);
- `qt_workbench_title_row_scale_*` and `qt_workbench_window_scale_*`: `no
  review-bar control folds into an overflow menu at any width` ([[BPR-1255]]).

No rendered row moved with the change from Consolas, so nothing was
re-baselined and no threshold changed.

**The share sheet's code.** The review-cycle view course does not register
the bundled faces. Only `main.cpp` does, at startup, so the course runs on the
native fallback (Consolas). A temporary probe, not committed, called
`register_bundled_code_font()` at the course's start, as the application does.
`qt_review_cycle_view_1_0` and `_2_0` then printed, for the opened note's
excerpt:

```
family=JetBrains Mono NL resolved=JetBrains Mono NL fixed=1 spacing=16 leading=0 height=172 max=0
```

Both rows pass that way (7.96 and 14.24 s). The excerpt resolves to "JetBrains
Mono NL", it is fixed pitch, and the row-height fit holds at its 16-pixel line.

### The review-cycle courses run on the bundled font (bpr-main-win, 2026-10-03)

The review-cycle view course and `review_cycle_desktop_public_path` build
their windows without `main.cpp`, so they ran on the native fallback. Both now
call `register_bundled_code_font()` at startup, as the application does, and
expect it to succeed. Each registers before it builds any appearance, so the
share sheet's code is "JetBrains Mono NL" there (measured above with the same
registration).

JetBrains Mono NL has no negative leading, so the courses no longer reach the
case BPR-1161's row-height fix answers: Windows' Arial, with line spacing 13
against height 14. A dedicated check, `fit_rows_at_negative_leading()` in the
view course, keeps it covered. It shows a ten-row excerpt in the first
installed face with negative leading (Arial on Windows) and expects no
scrolling. Where no such face is installed it says so and does not pass by
default.

Measured on Windows (MSVC 14.51, Qt 6.11.2, `build/windows-dev` Debug):

- With `fitted_height()` temporarily put back to `lineSpacing()` alone, both
  view rows failed at "a face with negative leading still shows all ten rows,
  no scrolling". So the check reaches the defect.
- With the fix, `qt_review_cycle_view_1_0` passes in 8.05 s, `_2_0` in
  14.23 s, and `application_review_cycle_desktop_public_path` in 13.50 s. The
  build printed no warning, and `clang-format` is clean.

### Linux at `6e220f1f3`: courses register the bundled font (bpr-main-linux, 2026-10-03)

Includes `6e220f1f3`. `cmake --build --preset dev` exits 0 with 0 `warning:`
lines. `ctest --preset dev -j4 --output-on-failure -R
"^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$|^qt_bundled_code_font$|^application_code_font$"`
passes **5 of 5**:

- `qt_bundled_code_font` 0.05 s and `application_code_font` 0.05 s
- `qt_review_cycle_view_1_0` 1.50 s and `_2_0` 3.11 s
- `application_review_cycle_desktop_public_path` 2.85 s

**The negative-leading row-height pin was unchecked on Linux.** Both view rows
print `negative-leading row fit: no installed face has negative leading here,
so not checked`. A probe, not committed, of the pin's own family list at 9 pt,
offscreen:

- Only **DejaVu Sans** is installed (`hasFamily=1`), and its leading is **0**
  (height 13.95, line spacing 13.95).
- Arial, Segoe UI, Tahoma, Verdana, Liberation Sans and Helvetica are absent
  (`hasFamily=0`) and resolve to DejaVu Sans.

So no face was used, and Linux gives the pin no case. Windows' Arial is the
face that exercises it.
