---
schema: "implement-rfc.card.v1"
id: "BPR-0747"
title: "A comment after a null directive paints as an error"
status: "In Progress"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"c0d3c5da","found_by":"attended-prep-2026-09-10 d20, while verifying BPR-0744"}
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
