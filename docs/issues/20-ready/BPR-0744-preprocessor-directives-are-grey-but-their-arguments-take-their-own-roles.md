---
schema: "implement-rfc.card.v1"
id: "BPR-0744"
title: "Preprocessor directives are grey but their arguments take their own roles"
status: "Ready"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decisions":["fed7a1bf-e20c-4731-ad72-387b31d29b98","4effa6f2-84e6-459c-8491-0e7e7c2ab3ca"]}
---

# Preprocessor directives are grey but their arguments take their own roles

The owner, 2026-09-11, in the v008 attended session, on two candidates.
Rejecting `BPR-0720-dark-palette` (decision `fed7a1bf`, 04:28:03Z), verbatim:

> comments are green, things are mostly right, but preprocess being grey is
> taken too far compared to the reference. The preprocessor directives
> themselves should be grey but not necessarily their arguments. Macros in
> preprocessor should still color as macros (purple). Strings in #include
> statements should color as the reference image

Rejecting `BPR-0738-identifier-colours` (decision `4effa6f2`, 04:36:00Z),
verbatim:

> As I answered in another question, most things are good now but include
> paths should be painted like the reference and things like macros in
> #ifndef MACRO should be painted as macros.

The screenshot is
[stdexec's `__detail` header in Read](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-dark-palette-preprocessor-2026-09-11.png):
`#include "../../stdexec/__detail/__config.hpp"`, `#if STDEXEC_USE_MODULES()`,
`#else`, `# include <concepts>` and `#endif` are grey end to end. The
reference is
[Visual Studio 2022 dark with Visual Assist](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-visual-studio-reference-2026-09-11.png),
where `#ifndef`, `#define`, `#include` and `#pragma` are grey, the macro name
after `#ifndef` and `#define` is purple, and the include path is
string-salmon.

This settles the first of the two open questions BPR-0738 carried: the
include path is painted like the reference, not grey. It also revises the
2026-09-11 owner direction recorded on RFC-BPR-0006, whose parenthetical
reads "grey preprocessor lines including the include path"; that stays true
of the directive and no longer of its arguments.

## What the code says

The classifier can already emit different roles for different ranges inside
one line: `span_collector::applyFormat` is called once per format range
KSyntaxHighlighting reports, at arbitrary sub-line offsets, and appends a
span per range
(`src/adapters/syntax/src/packaged_syntax_classifier.cpp:426-454`); adjacent
equal-role ranges are merged afterwards (`:497-506`). Two things collapse a
directive line anyway:

- The packaged C++ grammar carries `attribute="Preprocessor"` as the default
  attribute of its preprocessor contexts, so `#ifndef MACRO` and
  `#if STDEXEC_USE_MODULES()` arrive as a single Preprocessor range
  (`build/.cpm-cache/bprksyntaxhighlighting/8f28/data/syntax/isocpp.xml:748-750`,
  `:753-757`, `:770-775`, and `:779-786` where `Define` gives the macro name
  `attribute="Preprocessor"` at `:785`).
- Our own identifier post-pass runs only over `syntax_normal` runs
  (`packaged_syntax_classifier.cpp:435-448`): a Preprocessor run is appended
  and returned before `classify_identifiers` (`:458-484`) and
  `identifier_role` (`:365-366`, `is_macro_case` at `:240-255`) can look
  inside it. That single gate is why `#ifndef MACRO` never reaches the
  ALL_CAPS rule that already exists.

The include path is a separate question. The grammar gives `<…>` and `"…"`
inside an `#include` the `Prep. Lib` attribute, which maps to `dsImport`
(`isocpp.xml:741-746`, `:910`), and the classifier maps `Import` to
`core::appearance_role::syntax_import`
(`packaged_syntax_classifier.cpp:76-77`), painted `A3A3A3` on dark
(`src/adapters/qt/src/appearance_profile.cpp:180`). Repainting
`syntax_import` itself would repaint `import`, `from` and `@import` in
roughly fifty-five other packaged grammars (python, java, javascript,
typescript, kotlin, swift, css/less/scss, cmake, makefile, objectivec and
the rest), which is not what the owner asked for. Mapping `Import` to
`syntax_string` only where the identifier heuristics already gate on C and
C++ (`:124-128`) has no collateral outside those definitions and is the
narrower change.

## Scope

- A preprocessor directive keyword (`#include`, `#define`, `#ifndef`,
  `#ifdef`, `#if`, `#else`, `#endif`, `#pragma`) stays `syntax_preprocessor`
  grey. Its arguments do not inherit that role: an ALL_CAPS word in a
  directive takes `syntax_macro`, the role BPR-0738 added, and paints the
  same purple a macro paints in ordinary code.
- An `#include` path, in angle brackets or quotes, paints as the reference
  paints it. Prefer the C/C++-scoped classifier mapping to `syntax_string`
  over repainting `syntax_import`; if the slice repaints a role instead, it
  says on this card which and why, and what it does to the other fifty-five
  grammars.
- Decide, and record, what happens to the rest of a directive's text that is
  neither the keyword nor a macro nor a path: `once` after `#pragma`, the
  operators in `#if A && B`, a numeric literal. Grey is an acceptable answer;
  an unexamined one is not.
- Prefer the post-pass over forking the grammar. Editing the packaged
  definition moves `BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256` and the offline
  cache contract (`cmake/BprSyntaxHighlightingProvider.cmake:230,262-266`)
  and is out of proportion to the change.
- The light and high-contrast profiles are untouched, and the diff colour
  themes stay off-limits.
- Owned paths: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`,
  `tests/syntax_contract/syntax_classification_contract_tests.cpp` with
  `tests/fixtures/syntax/identifier_roles.cpp`, and
  `src/adapters/qt/src/appearance_profile.cpp` with
  `tests/adapters_qt/appearance_profile_tests.cpp` only if a dark value
  moves.

## Blast radius

No appearance store schema move: schema 5
(`src/adapters/sqlite/src/appearance_settings_database.hpp:29`) exists
because BPR-0738 added a role
(`appearance_settings_database.cpp:306-316`); a role-to-range change adds no
role and changes no column, so `palette_stored` is untouched and there is no
second reset for the owner. No customisation panel change: it iterates
`appearance_role_count` and labels from `stable_name`. No composition digest
repin unless `src/app/src/github_application.cpp` is edited, which this slice
does not.

## Acceptance

- `syntax_classification_contract` pins, over
  `tests/fixtures/syntax/identifier_roles.cpp`, that in `#ifndef MACRO` and
  `#define MACRO` the directive keyword is `syntax_preprocessor` and the name
  is `syntax_macro` at its own exact range, and that an `#include` path is
  the string role at its own range while the `#include` keyword stays grey.
  The existing assertions at
  `tests/syntax_contract/syntax_classification_contract_tests.cpp:664-669`
  ("a directive is not a macro", one span over `#define
  BEGIN_TEST_METHOD_PROPERTIES`) are the ones the owner's direction reverses;
  replace them rather than adding beside them.
- `test_the_dark_palette_follows_the_visual_assist_scheme`
  (`tests/adapters_qt/appearance_profile_tests.cpp:295-330`) is updated for
  whatever value the include path takes, and `core_appearance_contrast` and
  the headroom ratchet (`appearance_profile_tests.cpp:2158-2210`, dark at one
  hundredth at `:2176`; the 4.5:1 floor at `:71` and `:366-372`) pass. The
  contrast of the chosen path colour on the added and removed row fills,
  through the selection wash, is measured before it is pinned.
- `qt_appearance_*`, `qt_syntax_pane_composition`,
  `sqlite_appearance_settings_storage`, `application_appearance_*`,
  `application_composition_contract` and `architecture_boundaries` pass.
- The owner accepts a C++ header on the dark palette in an attended session:
  this is the criterion BPR-0720 and BPR-0738 both still carry, asked once
  more on a bundle with these ranges.
