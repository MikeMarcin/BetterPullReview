---
schema: "implement-rfc.card.v1"
id: "BPR-1727"
title: "The head pane edits the selected lines in place with the shared editor"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1726"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723","BPR-1724"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2; the pane keeps the product's own palette and type","milestone":"RFC-BPR-0090 M2"}
acceptance: [{"criterion":"AC1: RFC AC01: from each entry, on a selection or the caret, the head pane becomes editable with unchanged text, scroll, numbers and colours and the caret at the end of the last selected line; refused entries say why","outcome":"pass","revision":"e249fc5f674234d622de27583f77d4ecf0a999fc","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_(conversation_reveal|line_draft)$' (QT_QPA_PLATFORM=offscreen)","detail":"test_compose_in_the_drawer: the Suggest change action on line 13 opens the editor over the head pane at its rect, focused, the range 12..12, the caret at the line's end, the head pane's row pitch, line 6 at its own row, no block in the drawer or the body, and the status line names the line. test_return_in_the_code_starts_a_draft_on_the_selection (Shift+Return on three keyboard-selected lines), test_right_click_on_any_line_and_on_base_lines and test_line_numbers_select_lines (the menu on a drag) open exactly the selected lines. Refusals: a line outside every hunk (menu reason 'Suggestions need a changed region', in split and one pane), lines outside a hunk and a base line say why. In one pane the head file shows alone and the interleaved rows return after Esc."}},{"criterion":"AC2: RFC AC02: RC55-07's gestures work inside the range on the composed window, an edit to a fixed byte is refused with the status line's words, Enter grows the range and emptying it is a deletion","outcome":"pass","revision":"e249fc5f674234d622de27583f77d4ecf0a999fc","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_(conversation_reveal|line_draft)$' (QT_QPA_PLATFORM=offscreen)","detail":"Keys through the window's own key path (QWindowSystemInterface, so window shortcuts get their chance): Backspace and '3' write the block with its tab; 'x' on line 6 is refused whole, the edge flashes and the status line says 'Only line 13 is in this suggestion. Esc ends the edit.'; Enter grows the range to two rows, a wheel over the edit scrolls the comparison, and with the range above the top head line 21 is the edit's row 22 at the same height; two Backspaces take the row back. test_the_pane_edit_takes_the_editor_gestures: an Alt+Shift+Down box over the range, an IME composition then its commit at every caret, Undo, Paste at every caret, and a box reaching past the range refused whole. Emptying the range as a deletion is the core's (core_pull_request_suggestion at 860b7726f) and its 'Deletes lines a–b' note is painted by the editor."}},{"criterion":"AC3: Qt tests run offscreen; format, tidy on the changed files, the command-correctness reseal and the documentation gate pass","outcome":"pass","revision":"e249fc5f674234d622de27583f77d4ecf0a999fc","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest -R '^qt_workbench_' and '^(qt_conversation_cards_view|qt_diff_link_map_scale_.*|qt_diff_workbench_scale_.*|qt_suggestion_editor_.*|qt_review_cycle_view_.*|command_correctness_registry_contract|architecture_boundaries|architecture_boundary_contract)$'; format_check.py; tidy.py --warnings-as-errors; reseal_command_correctness.py; reseal_round_trip_composition.py --check; verify_documentation.py","detail":"21 of 21 window courses and the 14 related suites pass. clang-format 22.1.8 check passes. clang-tidy passes on every changed source unit; in workbench_window_tests.cpp it reports only two std::endl lines in the case tracer from b059676cc7, outside this change. The workbench_window_tests.cpp pin and the --line-draft inventory (two new cases) were resealed; the round-trip pins are unchanged (no src/app change). The documentation gate fails only on the provisional IDs and NEXT_ID's existing high-water report. rfc0016_test_inventory fails on 'ctest-inventory-command-absent:gh_cli_git_smart_transport' in this lean configuration, which this change does not touch."}}]
---

# BPR-1727: The head pane edits the selected lines in place with the shared editor

RFC-BPR-0090 M2 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)). The shared editor of RC55-07 over
the head pane at the same geometry, reused from Review Cycles.

## Scope

- `qt::suggestion_editor` gains the editable span, an external scroll offset
  so it follows the comparison, a number gutter and row fills, and keeps its
  behaviour when none is set.
- `diff_workbench` puts it over the head pane (split) or shows the head file
  alone (one pane), dims ribbons that touch or follow the range, and ends it.
- Every Suggest change entry (Shift+Return, the code menu, the review bar)
  opens it after `decide_suggestion_range`.

## Acceptance criteria

- AC1: RFC AC01: from each entry, on a selection or the caret, the head pane
  becomes editable with unchanged text, scroll, numbers and colours and the
  caret at the end of the last selected line; refused entries say why.
- AC2: RFC AC02: RC55-07's gestures work inside the range on the composed
  window, an edit to a fixed byte is refused with the status line's words,
  Enter grows the range and emptying it is a deletion.
- AC3: Qt tests run offscreen; format, tidy on the changed files, the
  command-correctness reseal and the documentation gate pass.

AC1 said "the selection kept" when the card was written. The edit puts the
caret at the end of the last selected line instead, so typing adds to the
lines rather than replacing them; RFC AC01 says the same since `e249fc5f6`.

## What was built

- The overlay is the Review Cycles editor itself, holding the whole head
  file. The core's editable span (M1) decides what may change; the editor
  only presents it.
- The drawer link that M3 builds on landed here too, because without it
  Suggest change would have no body: the field holds the words, the body is
  `compose_suggestion_body`'s, "What it changes" reads the code, and the
  draft stays linked after Esc so the block never moves into the field.

## Observations

- In this fixture the base and head panes sit 8 px apart after a wheel with
  no edit open; the edit follows the head pane exactly, so it inherits that.
- An attended look is still owed (RFC AC10, M4).

## Rebase

Rebased onto main `7075954df` on 2026-10-21. The rebase changed only main's own files (the diagnostics zip, release maintenance and the BPR-1715 storage follow-up), none of which this card's evidence reads, so the recorded revisions are the rebased commits: `489eff8f0` is `860b7726f`, `0f1b446ea` is `e249fc5f6`, `b13164f02` is `a7b8b0f6d` and `56bfc1518` is `9cf0d41b2`.
