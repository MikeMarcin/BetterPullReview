---
schema: "implement-rfc.card.v1"
id: "BPR-0726"
title: "Functions, types and macros are uncoloured in C and C++"
status: "Ready"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"0f243620-5510-45de-9e45-41b07c2b0265"}
---

# Functions, types and macros are uncoloured in C and C++

The owner rejected `BPR-0720-dark-palette` in the v006 attended session
(decision `0f243620`, 18:39:22Z): "Missing a lot of coloring, including
class, functions. macros". The screenshot
([owner-dark-palette-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-dark-palette-2026-09-11.png))
shows `ScreenBufferTests.cpp` on the dark palette with comments green,
keywords blue, strings salmon and numbers coloured, while `ProcessString`,
`GetTextBuffer`, `ScreenBufferTests`, `TextAttribute`, `ServiceLocator`,
`Log::Comment`, `VERIFY_ARE_EQUAL`, `BEGIN_TEST_METHOD_PROPERTIES` and the
`L` string prefix render in the plain `DCDCDC` ink.

## Cause

The palette is not at fault; the roles are never emitted. The classifier
(`src/adapters/syntax/src/packaged_syntax_classifier.cpp`) wraps
KSyntaxHighlighting 6.28.1 and only maps each upstream text style to a
role through `role_of` (:41-110); `span_collector::applyFormat` (:236)
drops normal-styled runs (:245). The upstream `isocpp.xml` grammar has no
Function style, types only its built-ins and `*_t`/`*_type` names,
variables only `m_*`, `g_*` and `s_*`, and declares its CONSTS/MACROS and
Internals items `dsNormal`, so every user identifier, PascalCase type and
ALL_CAPS macro arrives as `syntax_normal` and is painted with the default
ink (`src/adapters/qt/src/diff_workbench.cpp:2126-2140`). The palette
roles function, constant and annotation are never produced for C++. The
only classification test, `tests/syntax_contract/syntax_classification_contract_tests.cpp`
over `multiline_comment.cpp`, pins comment and string spans alone.

## Scope

- A post-pass over the normal-styled runs of the C and C++ definitions,
  never inside comments or strings: an identifier followed by `(` is a
  function; an identifier before `::`, after `class`, `struct`, `enum`,
  `typename` or `new`, or a PascalCase identifier followed by another
  identifier, `&`, `*` or `<` is a type; an ALL_CAPS identifier (two or
  more characters, digits and underscores allowed) is a macro, painted
  with the preprocessor role or a constant role if a separate macro colour
  is wanted; the `L`, `u8`, `u`, `U` prefixes belong to the string. Judge
  the heuristics against the owner's file, not a toy.
- The span cache key in `src/app/src/github_application.cpp` (around
  :10455, `token_span_cache_key`) must change with the heuristic so cached
  spans are not served stale.
- The light and high-contrast palettes already carry these roles; no
  palette change is expected. The Visual Assist colours for the new
  classes are BPR-0720's function `DCDCAA`, type `4EC9B0`, preprocessor
  `A3A3A3`.
- Owned paths: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`
  and its header, `src/app/src/github_application.cpp` (cache key only),
  `tests/syntax_contract` and a new C++ fixture beside
  `multiline_comment.cpp`.

## Acceptance

- The syntax contract pins, on a C++ fixture with a class, a member call,
  a free call, a scoped type, a PascalCase declaration, an ALL_CAPS macro
  and an `L"…"` literal, the function, type and macro roles at their
  exact ranges and no role inside comments or strings; it fails at HEAD.
- `syntax_contract`, the `diff_workbench` selectors and
  `qt_composed_github_scale_100` pass; a cached span set from before the
  change is not served (cache-key test or a documented invalidation).
- The owner accepts `ScreenBufferTests.cpp` on the dark palette in the
  signed bundle; the `BPR-0720-dark-palette` candidate is asked again on
  that bundle.
