---
schema: "implement-rfc.card.v1"
id: "BPR-0645"
title: "Bind a stored palette to the variant it customizes"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"AC1 a record whose palette equals a built-in palette produces zero overrides in every resolved variant; a light customization never colours the dark variant","outcome":"pass","revision":"8280c964","evidence":{"kind":"command","detail":"qt_appearance_profile passed with test_a_stored_palette_customises_exactly_one_variant and test_editing_a_colour_belongs_to_the_variant_on_screen; application_appearance_host passed with test_an_adopted_palette_records_the_variant_it_customises. Overrides are diffed against the stored variant's built-in profile and applied only while that variant resolves."}},{"criterion":"AC2 format-1 records migrate with the derived variant and unchanged typography","outcome":"pass","revision":"8280c964","evidence":{"kind":"command","detail":"sqlite_appearance_settings_storage passed with test_a_format_one_record_migrates_with_its_derived_variant (system, dark and high_contrast_light modes), test_the_stored_variant_round_trips and test_an_unknown_variant_name_is_refused. The migration rebuilds the settings table because ALTER TABLE on a STRICT table runs SQLite's quick check through the pragma table-valued function the hardened connection policy refuses."}},{"criterion":"AC3 focused selectors and the architecture boundary gate pass","outcome":"pass","revision":"8280c964","evidence":{"kind":"command","detail":"18 of 18 focused selectors passed in 373.16 s (ctest --test-dir build/dev); VerifyCoreBoundary.cmake and VerifyArchitectureBoundaries.cmake passed; clang-format --dry-run --Werror on every touched file and git diff --check exited 0; verify_documentation.py reported DOCUMENTATION_OK."}},{"criterion":"AC4 the tests-disabled Mac app on a dark desktop renders toolbar, header, fields and panels from one dark palette (attended capture)","outcome":"pass","revision":"8280c964","evidence":{"kind":"observation","detail":"The rebuilt, re-signed tests-disabled bundle launched on the dark macOS desktop (AppleInterfaceStyle Dark); a window-only capture showed title bar, toolbar, session header, locator fields, drawers, epoch controls, comparison panes and status line all rendered from the dark palette with no light region. The live appearance store migrated to format 2 with palette_variant light and unchanged typography. The capture was not retained."}}]
---

# Bind a stored palette to the variant it customizes

Owner direction, 2026-09-06: the workbench renders half light and half dark on
a dark macOS desktop and is unusable; this is the first slice of the
owner-directed UX track and the explicit post-pack frontier exception for
RFC-BPR-0018 recorded in [private-alpha.md](../../development/private-alpha.md).

Observed: `appearance_settings` holds `mode=system` and a stored palette that
is byte-for-byte the built-in light palette (46 of 46 roles equal). Under
`qt::appearance_profile`'s constructor the stored palette is read as overrides
against the built-in profile of the variant the *current* observation
resolves, so on a dark desktop all 46 roles differ and pin light colours over
the dark built-in. Widgets painted from the inherited `QPalette` render light
while the native toolbar, title bar and text fields follow the dark desktop.
This is BPR-0216's defect reappearing for stored records.

## Scope

A stored palette customizes exactly one variant. Persist that variant with the
record (`palette_variant`, schema format 2, migration deriving the variant of
existing rows from their mode under an empty observation, which is what the
old reading assumed). The Qt profile computes overrides only against the
built-in palette of the stored variant and applies them only while that
variant is resolved; any other resolved variant renders its built-in palette
untouched. Palette adoption records the variant in effect. Owned paths:
`src/application` appearance ports and host, `src/adapters/sqlite` appearance
settings database/store, `src/adapters/qt` appearance profile, their focused
tests, and the appearance architecture notes. The attended capture showed a
second cause with the same symptom, so the slice also owns the app
composition's native scheme observation in `src/app/src/github_application.cpp`
and the composition digests it repins.

## Acceptance

- A record whose palette equals a built-in palette produces zero overrides in
  every resolved variant; a light customization never colours the dark variant.
- Format-1 records migrate with the derived variant and unchanged typography.
- `qt_appearance_profile`, `application_appearance_host`,
  `sqlite_appearance_settings_storage`, `qt_workbench_window_scale_100` and
  `qt_composed_github_scale_100` pass; the architecture boundary gate passes.
- The tests-disabled Mac app on a dark desktop renders toolbar, header, fields
  and panels from one dark palette (attended capture).

## Implementation and verification — 2026-09-06

Three causes shared the symptom. First, the durable appearance record carried one
palette with no variant, and the Qt profile read it as overrides against
whatever variant the current desktop resolved; the owner's record held the
built-in light palette byte for byte, so on a dark desktop it pinned 46 light
overrides over the dark profile. `application::appearance_settings` now
carries `PaletteVariant`; the SQLite store is schema format 2 with a
`palette_variant` column and a forward migration that rebuilds the settings
table (ALTER TABLE on a STRICT table runs SQLite's quick check through the
pragma table-valued function, which the hardened connection policy refuses)
and derives each existing row's variant from its mode under an empty
observation; the host records the variant in effect on palette adoption; the
profile diffs overrides against the stored variant's built-in profile, applies
them only while that variant resolves, and rebinds edits to the variant on
screen. Second, the app composition observed the native scheme once before
the event loop ran, when Qt 6.10's macOS theme still answers `Unknown`, which
the core treats as light, and `colorSchemeChanged` never reports that first
real answer. The composition now re-observes once after startup and, for an
`Unknown` hint, derives the scheme from the platform palette's window
lightness. Third, the composition adopts the loaded record into the profile through
`adopt_palette`, which banked the palette as the customization of the variant
on screen; a stored light palette therefore became the dark profile's
overrides again by that route. `adopt_palette` now takes the record's variant
(a gesture still omits it and edits the variant on screen), and the
composition passes `PaletteVariant`. A dev-build trace on this desktop showed
Qt already reporting Dark at startup, so the second cause is defensive; the
third was the live one. Native chrome and app-painted panels then follow one
palette.

Verification at this revision:

- `cmake --build --preset dev` clean. Focused selectors passed 18/18 in 373.16 seconds:
  `qt_appearance_profile` (new `test_a_stored_palette_customises_exactly_one_variant`,
  `test_editing_a_colour_belongs_to_the_variant_on_screen`,
  `test_a_record_adopted_with_its_variant_does_not_colour_another`),
  `application_appearance_host` (new
  `test_an_adopted_palette_records_the_variant_it_customises`),
  `sqlite_appearance_settings_storage` (new round-trip, unknown-name refusal and
  format-1 migration cases for system, dark and high-contrast-light modes),
  `core_appearance_policy`, `core_appearance_contrast`, `core_theme_exchange`,
  `qt_appearance_settings_panel`, `qt_workbench_window_scale_100`,
  `qt_composed_github_scale_100`, `application_native_appearance_probe_contract`,
  `application_appearance_theme_customization_contract`,
  `application_appearance_gesture_contract`,
  `application_appearance_store_fallback_contract`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (full application and
  production mutation-branch digests repinned for the composition edit) and
  `architecture_boundaries`.
- Both boundary scripts, `clang-format --dry-run --Werror` on every touched
  file, `git diff --check` and the documentation gate passed.
- The tests-disabled candidate rebuilt and re-signed. The owner's live
  appearance store migrated to format 2 with `palette_variant = light` on first
  launch and kept its typography. The rebuilt candidate launched on this dark desktop rendered the title bar, toolbar, header, fields, drawers, comparison and status line from one dark palette; the window-only capture was inspected and not retained.

Delegation: an rfc-worker-routine implemented the three storage, host and
profile layers and their tests from the diagnosed design; it stopped at its
turn budget before building. The integrator built, found the migration's
ALTER refused under the connection policy, replaced it with the table rebuild,
and added the composition observation fix after the attended capture.

Implementation commit: `8280c964`.
