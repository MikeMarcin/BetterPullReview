---
schema: "implement-rfc.card.v1"
id: "BPR-0747"
title: "A comment after a null directive paints as an error"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"c0d3c5da","found_by":"attended-prep-2026-09-10 d20, while verifying BPR-0744"}
acceptance: [{"criterion":"syntax_classification_contract pins that a comment after a null directive takes syntax_comment and a genuinely malformed directive still takes syntax_error","outcome":"pass","revision":"c061cfc1","evidence":{"kind":"command","detail":"null_directive_contract under C++ and C: comment span syntax_comment at exact range, # syntax_preprocessor, #define // and #bogus // controls syntax_error; classifier-only revert fails 0/1; two over-broad mutants each fail a control. build/r2-logs/bpr0747-ctest-commit.log, bpr0747-red-ctest.log"}},{"criterion":"qt_syntax_pane_composition and qt_appearance_* pass; no appearance role added, no palette value moved, no digest repin","outcome":"pass","revision":"c061cfc1","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R '^(syntax_classification_contract|qt_syntax_pane_composition|qt_appearance_.*)$' 5/5 passed at the commit; diff touches only classifier, its revision constant, the fixture and the contract; format/tidy/diff-check pass"}}]
---

# A comment after a null directive paints as an error

Found while probing [[BPR-0744]]'s classifier over a real header, not in that
slice's own change. It is pre-existing: the span count is identical at HEAD and
after BPR-0744.

A `//` comment on a line whose only content before it is `#` and whitespace
paints `syntax_error`, which the dark profile draws `F39B73`, a salmon red.
In Qt's own `qsystemdetection.h` (0-based line 67):

    #        // TARGET_OS_IOS is only available in newer SDKs,

classifies as

    line 67 col 9 len 49 role syntax_error '// TARGET_OS_IOS is only available in newer SDKs,'

This is legal C++ — a null directive followed by a comment — and Qt writes it
deliberately to keep a comment aligned with the directive nesting around it. It
occurs four times in that one header.

## What the code says

The packaged grammar, not ours. `isocpp.xml:714` `AfterHash` matches the `#`
with `attribute="Error"` and `fallthroughContext="#pop!LineError"`, and enters
`PreprocessorCmd` (`:723`), which carries the same attribute and fallthrough
and has no rule for `//`. So the rest of the line falls through to `LineError`
and arrives as `dsError`, which
`src/adapters/syntax/src/packaged_syntax_classifier.cpp` maps to
`core::appearance_role::syntax_error`.

## Scope

- A comment after a null directive paints as a comment.
- The fix belongs in the post-pass, not in the packaged grammar: editing
  `isocpp.xml` moves `BPR_KSYNTAXHIGHLIGHTING_SOURCE_SHA256` and the offline
  cache contract (`cmake/BprSyntaxHighlightingProvider.cmake:230,262-266`),
  which is out of proportion to this.
- Decide whether the null directive's `#` itself stays `syntax_error`. It is
  not an error either, but it is one character and a different question.
- Do not widen this into a general "recover from LineError" rule without
  saying what else reaches `LineError` legitimately; a real malformed directive
  should still read as wrong.
- Owned paths: `src/adapters/syntax/src/packaged_syntax_classifier.cpp`,
  `tests/syntax_contract/syntax_classification_contract_tests.cpp` with
  `tests/fixtures/syntax/identifier_roles.cpp`.

## Acceptance

- `syntax_classification_contract` pins, over a fixture line of the shape
  above, that the comment takes `syntax_comment` and that a genuinely
  malformed directive still takes `syntax_error`.
- `qt_syntax_pane_composition` and `qt_appearance_*` pass. No appearance role
  is added and no palette value moves, so there is no schema move and no
  digest repin.

## Evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

Implemented at `c061cfc1`. The rule is narrower than "recover from LineError":
an error run becomes `syntax_comment` only when it opens `//` and nothing but
blanks, one `#` and blanks precede it on the line. Probes of the packaged
`isocpp.xml` and `c.xml` grammars found the other ways into `LineError`, and
all of them stay errors. A bare `#` alone on a line, a stray `#`, an unknown
directive (`#bogus X // c`), `#endif junk`, `#else junk`, `#define // c` (no
macro name), `#ifdef` with no name, `# /* block */` and the `%:` digraph each
open their error run before any `//`, or carry a keyword between `#` and `//`.

Decision on the null directive's `#`: it is unchanged. In this card's shape
the grammar already paints `#` and its blanks as `syntax_preprocessor`, and
the contract now pins that. Only a bare `#` alone on a line is `syntax_error`.
That is a different one-character shape, and it shares its error state with a
stray `#` in code, so it is left as a separate question.

`identifier_heuristic_revision` moves from 3 to 4. The header's contract is
that every post-pass change raises it so cached span sets retire. The contract
test already checks that the published definition version carries it. No
appearance role or palette value moved and no digest was repinned. The fixture
gains three lines inside its `clang-format off` block, and no test hashes that
fixture.

| Criterion | Direct check | Result | Revision |
|---|---|---|---|
| `syntax_classification_contract` pins the comment case and a malformed-directive control | New `null_directive_contract` under both the C++ and C definitions. The comment span is `syntax_comment` at its exact range, `#` is `syntax_preprocessor`, and the `#define // …` and `#bogus … // …` controls stay `syntax_error`. With only the classifier reverted it fails `C++: FAILED: a comment after a null directive is a comment at its own exact range` (0/1). Two over-broad mutants (any error run starting `//`; any run after `#`) each fail a control. | pass | `c061cfc1` |
| `qt_syntax_pane_composition` and `qt_appearance_*` pass with no role, palette or digest move | `ctest --test-dir build/r2-dev --output-on-failure -R '^(syntax_classification_contract\|qt_syntax_pane_composition\|qt_appearance_.*)$'`: 5/5 passed after rebuilding at the commit. `format_check.py`, `tidy.py --warnings-as-errors` on the two changed sources and `git diff --check` pass. | pass | `c061cfc1` |

Ignored logs: `build/r2-logs/bpr0747-ctest-commit.log`, `bpr0747-red-ctest.log`,
`bpr0747-mutant-*-ctest.log`, `bpr0747-format-check.log` and `bpr0747-tidy.log`.
Residual limits: the recoloured line is one plain comment span, so comment
substyles such as `TODO` alerts are not applied there. A backslash-continued
null-directive comment recolours only its first line.
