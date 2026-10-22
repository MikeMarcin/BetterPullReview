---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH1"
title: "Core decides a selected-lines suggestion: range, editable span, plan, body, rebuild, convert and carry"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1723"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M1"}
acceptance: [{"criterion":"AC1: Each decision above has core tests covering its refusals, including the fixed-byte refusals at both ends of the span, growth, deletion, a block holding backticks, an endings-only edit, and a composed body whose label header edit changes only the header's bytes (RFC AC03 and AC05, core share)","outcome":"pass","revision":"860b7726fac2aeb94460ddc79de15ff05e65a18e","evidence":{"kind":"command","command":"ctest --preset core-only -R '^core_(pull_request_suggestion|feedback_suggestion_preservation|suggestion_text_editing|suggestion_draft|review_comment_range|filtered_comment)$'","detail":"6 of 6 pass. core_pull_request_suggestion covers the range refusals, the span (typing, Backspace at its start and Delete at its end refused, a second caret outside, clipping of a triple-clicked line and of a selection from a fixed line, Select All, growth by Enter and paste, undo of the extent, emptying, CRLF and no-final-newline files) and the plan (replacement, growth, deletion, unchanged, endings only, backticks, filtered reasons). Negative control: with the span's guard disabled the test fails at 'typing on a fixed line is refused'. core_feedback_suggestion_preservation runs a composed body through its whole header-edit matrix and checks it splits back to the same block."}},{"criterion":"AC2: A kept body rebuilds the span exactly, and an unparsable block, a second block and another comparison are refused (RFC AC06, core share)","outcome":"pass","revision":"860b7726fac2aeb94460ddc79de15ff05e65a18e","evidence":{"kind":"command","command":"ctest --preset core-only -R '^core_pull_request_suggestion$'","detail":"test_a_body_splits_back_exactly and test_a_kept_draft_opens_again: compose and split round-trip byte for byte for five proses and an empty block; hand-typed variants stay text; a rebuilt span plans back to the same body, and a deletion to a deletion; not_composed, second_block, other_comparison and no_original are each refused."}},{"criterion":"AC3: Carry is decided from the old-to-new head correspondence exactly (RFC AC08, decision), and a filtered line is placeable only when it is unchanged at head (RFC AC09)","outcome":"pass","revision":"860b7726fac2aeb94460ddc79de15ff05e65a18e","evidence":{"kind":"command","command":"ctest --preset core-only -R '^core_pull_request_suggestion$'","detail":"test_carry_needs_every_line_unchanged: unmoved and moved lines carry; a changed line, lines split by an insertion and an inverted range do not. test_the_plan_is_what_github_receives: head placement is placeable; earlier_commit, file_comment and refused give changed_after_commit, unresolvable_line and not_placed."}},{"criterion":"AC4: core-only builds and its tests pass; format, tidy on the changed files and the documentation gate pass","outcome":"pass","revision":"860b7726fac2aeb94460ddc79de15ff05e65a18e","evidence":{"kind":"command","command":"cmake --preset core-only (-g0, shared CPM cache); cmake --build --preset core-only; ctest --preset core-only; _coding-standard/scripts/format_check.py <changed>; _coding-standard/scripts/tidy.py --build-dir build/core-only --warnings-as-errors --paths <changed .cpp>; scripts/verify_documentation.py","detail":"310 of 314 core-only tests pass; the four that fail also fail on main 07a9594c3 without this change: documentation_structure, documentation_issue_graph_contract and issue_id_allocation_contract need the .agents link a worktree lacks (they pass through the documentation script's link), and sqlite_review_mutation_recovery_storage fails 'v3 migrates transactionally without altering native row' on unchanged code. clang-format 22.1.8 check and clang-tidy pass. The documentation gate fails only on the provisional BPR-PENDING-AH IDs and NEXT_ID's existing high-water report; every other check passes."}}]
---

# BPR-PENDING-AH1: Core decides a selected-lines suggestion: range, editable span, plan, body, rebuild, convert and carry

RFC-BPR-0090 M1 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)). The core decisions, as pure functions
of values the host already holds, with core tests that run under `core-only`.

## Scope

- `decide_suggestion_range`: head side, one changed region (a single line
  too), and no range on a filtered diff whose right side is not head.
- The editable span in `suggestion_text_editor`: edits that would change a
  fixed byte are refused whole, selections into fixed text are clipped,
  the range grows and shrinks with line breaks, and its extent is part of
  undo.
- `plan_pull_request_suggestion`: changed or not, anchor, block, composed
  body, what it changes, and placeable or the reason.
- `split_suggestion_body`, `rebuild_suggestion_span`,
  `convert_suggestion_to_text` and `decide_suggestion_carry`.

## Acceptance criteria

- AC1: Each decision above has core tests covering its refusals, including
  the fixed-byte refusals at both ends of the span, growth, deletion, a block
  holding backticks, an endings-only edit, and a composed body whose label
  header edit changes only the header's bytes (RFC AC03 and AC05, core share).
- AC2: A kept body rebuilds the span exactly, and an unparsable block, a
  second block and another comparison are refused (RFC AC06, core share).
- AC3: Carry is decided from the old-to-new head correspondence exactly
  (RFC AC08, decision), and a filtered line is placeable only when it is
  unchanged at head (RFC AC09).
- AC4: `core-only` builds and its tests pass; format, tidy on the changed
  files and the documentation gate pass.

## Verification

At `860b7726f`, macOS, lean build (`-g0`, the main checkout's CPM cache):

- `core-only`: 310 of 314 tests pass. The four that fail also fail on main
  `07a9594c3` without this change: three need the `.agents` link a worktree
  lacks, and `sqlite_review_mutation_recovery_storage` fails "v3 migrates
  transactionally without altering native row" on unchanged code.
- The six suggestion and comment suites pass, and `core_pull_request_suggestion`
  fails with the span's guard disabled.
- clang-format 22.1.8 check and clang-tidy (warnings as errors) on the
  changed files pass.
- The documentation gate fails only on provisional IDs and NEXT_ID's
  existing high-water report.

## Notes for M3

A composed body with no prose opens with its fence, and the header edit
refuses to put a label in front of a fence (`restructuring_required`). The
drawer's label chip therefore applies to the prose field, which M3 composes
with the block, rather than to the whole body.

## Rebase

Rebased onto main `7075954df` on 2026-10-21. The rebase changed only main's own files (the diagnostics zip, release maintenance and the BPR-1715 storage follow-up), none of which this card's evidence reads, so the recorded revisions are the rebased commits: `489eff8f0` is `860b7726f`, `0f1b446ea` is `e249fc5f6`, `b13164f02` is `a7b8b0f6d` and `56bfc1518` is `9cf0d41b2`.
