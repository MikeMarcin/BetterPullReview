---
schema: "implement-rfc.card.v1"
id: "BPR-1432"
title: "Count source lines as Git does, so a lone CR stays inside its line"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3dc0cdb86","evidence":{"kind":"observation","detail":"core_diff_compute pins a\\r\\nb\\rc\\n as two lines with b\\rc content; text_line_ending::cr removed and refused by validation.","path":"docs/issues/60-done/BPR-1432-count-lines-as-git-does.md"}},{"criterion":"AC2","outcome":"pass","revision":"3dc0cdb86","evidence":{"kind":"observation","detail":"core_suggestion_text_editing, core_review_cycle_feedback, core_review_cycle_share and core_review_cycle_suggestion pin the LF-only count.","path":"docs/issues/60-done/BPR-1432-count-lines-as-git-does.md"}},{"criterion":"AC3","outcome":"pass","revision":"3dc0cdb86","evidence":{"kind":"observation","detail":"qt_diff_workbench_scale_100/125/200 test_a_lone_cr_is_shown_inside_its_line: one block per decoded line, line two reads two U+240D three; qt_suggestion_editor_1_0/2_0 pass.","path":"docs/issues/60-done/BPR-1432-count-lines-as-git-does.md"}},{"criterion":"AC4","outcome":"pass","revision":"3dc0cdb86","evidence":{"kind":"observation","detail":"core_source_matcher: a\\nb no longer matches across a lone CR, a\\rb matches literally, CRLF query matches LF; boundary, documentation, format and changed-line tidy gates pass.","path":"docs/issues/60-done/BPR-1432-count-lines-as-git-does.md"}}]
---

# Count source lines as Git does, so a lone CR stays inside its line

The 2026-10-13 audit found that the decoded diff ended a line at LF, CRLF
and a lone CR ([diff_compute.cpp](../../../src/core/src/diff_compute.cpp)),
while Git and GitHub end a line only at LF. A comment anchor sends the
decoded line index plus one as its GitHub `line`
([github_mutation_contract.cpp](../../../src/adapters/github/src/github_mutation_contract.cpp)),
so in a file with a lone CR every comment after it named the wrong GitHub
line. The suggestion editor and the Review Cycles feedback, share and
suggestion splitters split the same way, and Qt's document starts a block at
a lone CR, which is why the views agreed with the wrong count.

## Acceptance

- AC1: The decoded diff ends a line only at LF; a CR immediately before it
  forms a CRLF ending and a lone CR is line content. The `cr` line ending no
  longer exists, so no producer can emit it.
- AC2: The suggestion editor and the Review Cycles feedback, share and
  suggestion line counts follow the same rule, so a note's or suggestion's
  line agrees with the diff's line.
- AC3: Source panes, the unified view, line details and the suggestion
  editor show a lone CR as U+240D inside its line. Pane block numbers remain
  the decoded line numbers, so anchors and reveals are unchanged elsewhere.
- AC4: Find reads CRLF as LF and keeps a lone CR as content, in documents
  and in queries.

The diff algorithm revision is unchanged: it is part of the persisted file
comparison digest, and only files containing a lone CR change shape.
Copying from a pane copies the U+240D symbol, not the CR.

## Evidence

- AC1: `core_diff_compute` pins `a\r\nb\rc\n` as two lines (CRLF, then LF
  with `b\rc` as content); `text_line_ending::cr` is removed from
  `diff_compute.hpp`, and the validator no longer admits it.
- AC2: `core_suggestion_text_editing`, `core_review_cycle_feedback`,
  `core_review_cycle_share` and `core_review_cycle_suggestion` pin the same
  count for `project_lines`, `line_text_at`, `line_span_bytes`, excerpts and
  suggestion lines.
- AC3: `qt_diff_workbench_scale_{100,125,200}` adds
  `test_a_lone_cr_is_shown_inside_its_line`: both panes hold one block per
  decoded line and line two reads `two\u240Dthree`; `qt_suggestion_editor_*`
  pass with the painter's CR symbol.
- AC4: `core_source_matcher` pins that `a\nb` no longer matches across a lone
  CR, `a\rb` matches it literally, and a CRLF query still matches LF.
- Gates: the eight core suites above, five Qt suites, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract` and the
  documentation gate pass; clang-format 22 applied; clang-tidy 22 reports no
  finding on changed lines.
