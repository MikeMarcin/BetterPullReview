---
schema: "implement-rfc.card.v1"
id: "BPR-0738"
title: "Macros and functions take the Visual Assist colours on the dark palette"
status: "In Progress"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"7789e40f-d61d-4900-813c-f23c508db56a"}
---

# Macros and functions take the Visual Assist colours on the dark palette

The owner, 2026-09-11, rejecting `BPR-0720-dark-palette` in the v007
attended session (decision `7789e40f`, 23:45:40Z), verbatim: "It's better
but still off macros are still grey, functions arent orange etc". Three
screenshots:
[terminalOutput.cpp in Read](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-dark-palette-terminaloutput-2026-09-11.png),
[ScreenBufferTests.cpp in Read](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-dark-palette-screenbuffertests-2026-09-11.png)
and the reference the owner means,
[Visual Studio 2022 dark with Visual Assist](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-visual-studio-reference-2026-09-11.png).
[BPR-0726](../60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md),
the role classification, was accepted thirty-five seconds later; what is
rejected is the colour each role paints, not which words get a role.

## What the code says

- An ALL_CAPS macro is classified as `syntax_preprocessor`
  (`src/adapters/syntax/src/packaged_syntax_classifier.cpp:365-366`), the
  same role as an `#include` directive (`:62-63`), so macros paint the
  directive grey `A3A3A3` (`src/adapters/qt/src/appearance_profile.cpp:173`).
  Visual Studio's own C++ colouring does paint macros grey; Visual Assist
  does not, and the owner's reference is Visual Assist.
- Calls are `syntax_function`, painted `DCDCAA` (`:162`), Visual Studio's
  pale yellow; in the reference, functions are gold.
- `core::appearance_role` (`src/core/include/bpr/core/appearance_model.hpp:79-104`)
  has no macro role; the nearest are preprocessor, import, constant and
  attribute.

Colours sampled from the reference screenshot (the most frequent
non-background values in the code region, hex RGB): macros `BD63C5`
(purple: `FARADAY_WORKBENCH_APP_PRODUCTION_EVIDENCE_HPP`), functions
`FFD700` (gold: `run_authoring_scenario_contract`), a light-blue family for
variables and parameters (`7FC2FF`, `99EBFF`), `9184EE` for the
`std::string_view`/`path` type family, `00BFFF` for namespaces, `569CD6`
keywords, `D69D85` strings, `9B9B9B` for `#include`/`#define` directives.
These are what the owner sees, not a specification; the slice confirms each
role's value against the reference before pinning it.

## Scope

- Macros get their own colour on the dark palette, distinct from the
  directive grey: either a new `syntax_macro` role through the core model,
  the classifier, the palette, the customisation panel and the store (with
  the customisation schema moved if a stored palette gains a row), or the
  classifier maps ALL_CAPS identifiers to an existing role that is repainted
  for it. The slice chooses and records why; a new role is the honest one if
  the customisation panel is to let the owner tune it.
- `syntax_function` on the dark palette moves from `DCDCAA` to the
  reference gold, lightened or darkened only if it fails the 4.5:1 floor on
  the diff row fills (the contrast audit and headroom ratchet must pass).
- The other roles are compared against the reference in the same pass and
  any that are visibly off (types, variables, namespaces) are listed on
  this card with the values chosen; the light and high-contrast palettes
  are untouched, and the diff colour themes are settled and off-limits.
- No second reset of the owner's stored palette unless a schema move forces
  one; BPR-0720's one-time reset already ran on this Mac.
- Owned paths: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`,
  `src/adapters/qt/src/appearance_profile.cpp`,
  `tests/adapters_qt/appearance_profile_tests.cpp`
  (`test_the_dark_palette_follows_the_visual_assist_scheme`), the syntax
  contract fixtures, and the core role model, panel and store if a role is
  added.

## Acceptance

- `test_the_dark_palette_follows_the_visual_assist_scheme` pins the new
  macro and function values and fails on `A3A3A3`/`DCDCAA`; the syntax
  contract pins a macro's role at an exact range distinct from a directive's.
- `qt_appearance_*`, `syntax_contract`, the `diff_workbench` selectors,
  `application_appearance_theme_customization_contract` and
  `qt_composed_github_scale_100` pass; digests repinned if any drifts.
- The owner accepts `ScreenBufferTests.cpp` and `terminalOutput.cpp` on the
  dark palette in the signed bundle in an attended session; this is the
  criterion BPR-0720 has carried since v005, asked once more on a bundle
  with these values.

## Owner answer and corrected reading, 2026-09-11 (mandate r9)

Asked to confirm the reading above (macros purple, functions gold) and
whether a distinct macro role is acceptable, the owner answered, verbatim:
"yes adding a distinct macro rule is not only acceptable, it's necessary,
also pretty sure functions were orange in my screenshot that i provided.
It was class/struct types that were expected to be yellow like in the
screenshot."

The owner is right and the sample list above is wrong. The reference was
resampled per token (the most frequent non-background value inside each
identifier's box on the 3840x2088 capture):

| Token in the reference | Value | Role today | Dark value today |
|---|---|---|---|
| `FARADAY_WORKBENCH_APP_PRODUCTION_EVIDENCE_HPP` (macro) | `BD63C5` purple | `syntax_preprocessor` | `A3A3A3` |
| `run_authoring_scenario_contract`, `run_native_visual_evidence` (functions) | `FF8000` orange | `syntax_function` | `DCDCAA` |
| `QApplication`, `production_evidence_context`, `product_adapter_snapshot`, `std::string_view`, `path` (class, struct and alias types; the standard aliases also italic) | `FFD700` gold | `syntax_type` | `4EC9B0` |
| `faraday::workbench::app::detail` (namespace) | `B8D7A3` | whatever the classifier emits for a namespace segment, to be checked | |
| `application_name`, `context` (variables, parameters) | `BDB76B` khaki | `syntax_variable` | `9CDCFE` |
| `const`, `int`, `auto` | `569CD6` | `syntax_keyword`, `syntax_builtin` | `6DA9DB` (lightened for the floor) |
| `#include`, `#define`, `#pragma` | `9B9B9B` | `syntax_preprocessor` | `A3A3A3` |
| `<faraday/...>`, `"Faraday..."` | `D69D85` | `syntax_import` / `syntax_string` | `A3A3A3` / `D69D85` |
| `// This structure ...` | `57A64A` | `syntax_comment` | `66B559` |
| `[[nodiscard]]` | `4EC9B0` | `syntax_attribute` | `C8C8C8` |

The `FFD700` for functions and the `9184EE`, `7FC2FF`, `99EBFF` and
`00BFFF` values listed earlier were misattributed samples; nothing in the
reference carries them as a role colour.

Settled by this answer and the resample, and therefore the slice: a new
`syntax_macro` role through the core model, the classifier, the palette,
the customisation panel and the store, painted `BD63C5` on the dark
palette; `syntax_function` to `FF8000`; `syntax_type` to `FFD700`. Each is
lightened or darkened along its hue only if it fails the 4.5:1 floor on the
diff row fills. Variables to `BDB76B` under the same rule, since the
reference shows it plainly and the owner's "etc" covers roles beyond the
two named. Attributes to `4EC9B0` for the same reason. Not settled, and not
guessed: the include path (the reference paints it string-salmon, the
accepted BPR-0720 direction says "grey preprocessor lines including the
include path"), and whether the standard-library aliases' italic matters;
both are asked in the v008 packet rather than changed. The light and
high-contrast palettes are untouched.

## Notes

- Slice, 2026-09-11 (d16). `core::appearance_role` gains `syntax_macro`
  (before `syntax_preprocessor`; `appearance_role_count` 47, the contrast
  audit's syntax run 26), named `syntax_macro` in the exchange vocabulary,
  so the customisation panel lists it by itself (it iterates the count and
  labels with `stable_name`; no panel code changed). The C and C++
  classifier returns `syntax_macro` for an ALL_CAPS word; a directive line
  stays `syntax_preprocessor`, and `syntax_classification_contract` pins
  `VERIFY_ARE_EQUAL` as a macro and `#define BEGIN_TEST_METHOD_PROPERTIES`
  as not one.
- Dark palette values, from the per-token resample of the owner's
  reference, each checked against the 4.5:1 floor on every canvas
  background with and without the selection wash: `syntax_type` `FFD700`
  (worst pair 8.28), `syntax_function` `FF8000` (4.61), `syntax_variable`
  `BDB76B` (5.61), `syntax_attribute` `4EC9B0` (5.70), and `syntax_macro`
  `CE8AD4`: the reference's `BD63C5` reaches only 3.18 on the added fill
  under the wash, so it is lightened along its own hue, one step of
  lightness at a time, to the first value that clears the floor (4.54, the
  dark profile's new tightest pair at 4 hundredths over its recorded 1).
  Light and high-contrast palettes gain a `syntax_macro` row equal to their
  `syntax_preprocessor` value and change nothing else.
  `test_the_dark_palette_follows_the_visual_assist_scheme` pins the five
  values and that macros are not `A3A3A3` nor functions `DCDCAA`.
- The store: the stored palette is one theme document that the reader
  imports on every load and the core refuses when a role is missing, so a
  version-4 file would have failed to load whole (mode and fonts included)
  on the first launch of this build. `appearance_settings_schema_version`
  is 5 with the same columns; a version-4 document gains a `syntax_macro`
  line carrying its own `syntax_preprocessor` colour, which is what macros
  painted before the role existed, and `palette_stored` is untouched: no
  second reset. `sqlite_appearance_settings_storage` adds the version-4
  migration and version 4 to the legacy admission. An older build refuses
  the version-5 store.
- Not changed, asked in v008: the include path (still the directive grey,
  as the accepted BPR-0720 direction says; the reference paints it
  string-salmon) and the italic on standard-library aliases.
- Found at HEAD, not this slice: `application_composition_contract` was
  red since [BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md)
  landed (91383cc4) because `github_application.cpp` changed after its
  round-trip pins were last set: the full-file digest, the production
  manifest digest and the `->submit(` allowlist count (a third
  `SetupDestination->submit`, the Recently viewed open by the Home path).
  All three repinned here in `application_review_round_trip_composition_contract.cmake`;
  the v007 packet's "unchanged since d1ba791f" was wrong.
- Verification at this head: `cmake --build --preset dev` clean; 22 of 22 in
  `core_appearance_*`, `core_theme_exchange`, `qt_appearance_*`,
  `qt_syntax_pane_composition`, `qt_diff_workbench_scale_{100,125,200}`,
  `syntax_classification_contract`, `sqlite_appearance_settings_storage`,
  `application_appearance_*`, `application_diff_workbench_*`,
  `qt_composed_github_scale_100`, `application_composition_contract` and
  `architecture_boundaries` after the repins; strict tidy and
  `clang-format --Werror` clean on the five touched translation units;
  `verify_documentation.py` and `git diff --check` clean.
