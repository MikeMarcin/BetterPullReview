---
schema: "implement-rfc.card.v1"
id: "BPR-0726"
title: "Functions, types and macros are uncoloured in C and C++"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"0f243620-5510-45de-9e45-41b07c2b0265"}
acceptance: [{"criterion":"AC1 syntax contract pins function, type and macro roles at exact ranges on a C++ fixture and no role inside comments or strings; fails at HEAD","outcome":"pass","revision":"working tree over 41fbf3bf","evidence":{"kind":"command","detail":"ctest -R ^syntax_classification_contract$ passes with identifier_role_contract over tests/fixtures/syntax/identifier_roles.cpp; the contract references identifier_heuristic_revision, absent at HEAD, so it cannot pass there"}},{"criterion":"AC2 syntax_contract, diff_workbench selectors and qt_composed_github_scale_100 pass; a pre-change cached span set is not served","outcome":"pass","revision":"working tree over 41fbf3bf","evidence":{"kind":"command","detail":"syntax_classification_contract and qt_composed_github_scale_100 pass; the contract pins that a key carrying the pre-post-pass Version is refused as unavailable (in-memory cache only). qt_diff_workbench_scale_{100,125,200} are red at HEAD on a core diff compute_limit assertion BPR-0719 changed, independent of this change; repaired in the next commit of this dispatch and re-run there"}},{"criterion":"AC3 the owner accepts ScreenBufferTests.cpp on the dark palette in the signed bundle; BPR-0720-dark-palette re-asked","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0726-cpp-identifier-roles, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint f46a9ee3a42878247bec904a98a4bed721eb096aa92e9e2a7f996e35c6709d5b, decision 91d2fa40-e8da-47b9-9825-64e4b0f76a5e, actor human, 2026-09-11T23:46:15Z: accept. ScreenBufferTests.cpp on the dark palette: calls, types and ALL_CAPS macros carry roles; accepted without a note. The colour each role paints was rejected on BPR-0720 thirty-five seconds earlier ('macros are still grey, functions arent orange') and is carded as BPR-0738; the classification itself is what this criterion asked and it is accepted."}}]
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

## Verification record, 2026-09-11 (d11)

The post-pass lives in `span_collector::classify_identifiers`
(`src/adapters/syntax/src/packaged_syntax_classifier.cpp`), which reads only
the normal-styled runs the grammar reports, so comments, strings and the
`#define` line keep the grammar's own spans. `identifier_role` decides from the
line text: an ALL_CAPS word is a macro (preprocessor role) even when invoked,
which is how `VERIFY_ARE_EQUAL(...)` reads in the owner's file; a word before
`(` is a function; a word before `::` or after `class`/`struct`/`enum`/
`union`/`typename`/`new` is a type; a PascalCase word before another word,
`&`, `*` or `<` is a type. The grammar already paints `L"…"` as one string
span including the prefix and `std::wstring` as builtin; the post-pass never
overrides a styled run. Only the `C++` and `C` definitions run it.

Cache invalidation: `identifier_heuristic_revision` (header) is folded into
the upper 16 bits of those two definitions' published `Version` in `record_of`,
so every `token_span_cache_key` built before the change names a revision the
classifier refuses as `unavailable`; the span cache is in memory only, and
`github_application.cpp` needed no edit. Raising the constant retires the
cached spans of the next heuristic change.

Fixture `tests/fixtures/syntax/identifier_roles.cpp` and
`identifier_role_contract` in the syntax contract pin the roles at exact
byte ranges; the contract cannot compile at HEAD (the revision constant is
new), so it fails there by construction.

Red at HEAD, outside this card, found by the focused run:
`qt_diff_workbench_scale_{100,125,200}` and the diff-workbench public paths
fail on "real compute limit produces an unavailable result" (a core-only
assertion BPR-0719 changed the semantics of), and
`application_composition_contract` reports the `src/app` source-tree digest
drifted (no `src/app` file is touched here). Both are repaired separately
in this dispatch.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0726-cpp-identifier-roles` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: ScreenBufferTests.cpp on the dark palette: calls pale yellow, types teal, ALL_CAPS macros preprocessor grey, comments and strings untouched, misfires as much as misses.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0726-cpp-identifier-roles`, decision `91d2fa40`, 23:46:15Z: **accept**.
Accepted without a note. The values the roles paint are [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)'s criterion, rejected in the same round and carded as [BPR-0738](../30-in-progress/BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md); this card's classification is Done.
