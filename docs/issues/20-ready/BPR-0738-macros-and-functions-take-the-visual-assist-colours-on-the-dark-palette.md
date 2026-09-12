---
schema: "implement-rfc.card.v1"
id: "BPR-0738"
title: "Macros and functions take the Visual Assist colours on the dark palette"
status: "Ready"
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
