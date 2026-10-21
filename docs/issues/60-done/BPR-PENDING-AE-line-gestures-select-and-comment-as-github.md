---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AE"
title: "Line numbers, one pane, empty lines and Comment take a selection as GitHub does"
status: "Done"
rfcs: []
depends_on: ["BPR-1721"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner approval 2026-10-21 (relayed by the coordinator) of the four follow-ups BPR-1721 recorded","related_cards":["BPR-1212","BPR-1335","BPR-1682","BPR-1698","BPR-1721"],"related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"frontend_design":"frontend-design:frontend-design loaded before the gesture and menu wording; no new visual surface"}
acceptance: [{"criterion":"AC1: the one-pane comparison has the code's right-click menu on its code and its numbers, for the line each row shows (an unchanged or added row its head line, a removed row its base line), with Add comment, Suggest change, the range-labelled items and the reason row","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_one_pane_has_the_code_menu. Failing before: 'a right-click in one pane opens the code's menu on the clicked head line'."}},{"criterion":"AC2: a click on a line number selects its line, a Shift-click extends the selection to it and a drag over the numbers selects the lines it crosses, in both views; the \"+\" in its column at the number and code boundary still adds a comment, and a right-click on a number still opens the menu","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_line_numbers_select_lines, then test_the_plus_on_a_pull_request_without_threads_opens_its_draft and test_a_selection_takes_a_range_comment_from_its_menu unchanged. Failing before: 'split: a click on a line's number selects that line'."}},{"criterion":"AC3: a drag whose pointer ends on an empty line takes that line, in both views, and Shift+Down goes on from it","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_a_drag_onto_an_empty_line_takes_it. Failing before: 'split: a drag from line 5 that ends on the empty line 8 selects lines 5 to 8'."}},{"criterion":"AC4: Comment (Option-C) on several selected lines starts one draft on all of them, as Return and Add comment do, and names them; on one line it opens that line's draft","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_comment_takes_the_selected_lines. Failing before: 'Comment names the selected lines: Co&mment on line 3…', and with that check set aside, 'Option-C on lines 3 to 5 opens one draft on all three'."}},{"criterion":"AC5: related suites, gates and docs","outcome":"pass","evidence":{"kind":"command","command":"ctest --preset dev -R '<related suites>'; cmake --build --preset dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Results in the Verification section."}}]
---

# BPR-PENDING-AE: Line numbers, one pane, empty lines and Comment take a selection as GitHub does

## Origin

[[BPR-1721]] made a comment on several lines visible from the menu to the
published thread, and recorded four gaps it left: the one-pane comparison had
no code menu, a line number did not select lines as GitHub's does, a drag
ending on an empty line stopped at the line above, and Comment (Option-C)
opened the selected line's own draft rather than the selection's. The owner
approved fixing all four on 2026-10-21.

## What was found

1. **One pane had no code menu.** The window gave the code menu to the split
   view's two panes only. The one-pane source kept Qt's default text menu
   (Copy, Select All), so Add comment there was reachable only by Return, the
   "+" (which one pane has never shown) and Comment. The window also read a
   right-click's line as the clicked block on the pane's side, which in one
   pane is a row, not a line.
2. **Line numbers did not select.** A press on a number went nowhere unless
   it was a thread's tinted number, which opens the thread. Only the code
   took clicks and drags.
3. **A drag onto an empty line stopped above it.** A selection's last line was
   read as the block before the selection's end (`selectionEnd() - 1`), so a
   selection that only reaches the start of a line does not take it. On an
   empty line the start is the whole line, so wherever the pointer ended on
   it, the line was left out.
4. **Comment ignored the selection.** `comment_or_reply` opened the shown
   draft, which is the selected line's, and never read the selection's range,
   while Return, Add comment and the "+" take `comment_on_selected_lines`.

## Change

- **Gesture routing only.** The rules stay in the core: every path ends in
  `decide_comment_range` through `comment_on_selected_lines`, as before. The
  diff workbench reads which line a row, a number or a selection end is, and
  the window routes gestures.
- **One pane.** The window gives the one-pane source the same custom menu.
  The workbench's new `select_line_for_menu` reads the line from the pane: a
  split pane's block on its side, or in one pane the line a row shows (its
  head line, or the base line a removed row shows). It keeps the selection
  when the row is inside it and selects the row's line otherwise. The menu's
  caption names both ends when one-pane rows start on one side and end on the
  other ("Base line a to line b"), and the core's `mixed_side` refusal is the
  reason row.
- **Line numbers.** The number gutter hands a press, a Shift-press and a drag
  to the workbench's `select_numbered_line`. It selects the row's line, holds
  whole lines in the pane's text cursor from the selection's anchor (as Shift
  and the arrows do), and gives the code the keyboard, so Return, the menu and
  Comment take the lines. A thread's tinted number still opens its thread,
  and Shift on it extends. A press in the "+" column's part in the number's
  padding is the "+"'s even before the button is drawn, so it never selects
  instead of adding a comment. The keyboard movers now share the anchor and
  cursor helpers with the numbers.
- **Empty lines.** `covered_row` gives the row a selection's end covers: the
  line above when the end is at the start of a line with text, the line
  itself when that line is empty. It is bounded by the pane's rows, because a
  file's final newline leaves an empty block that is no line. It replaces
  `selectionEnd() - 1` in the range, the source range and both line cursors.
- **Comment.** On several selected lines Comment takes
  `comment_on_selected_lines`, as Return does, and its text says "Comment on
  lines a–b…". A thread the reviewer picked is still answered first, and one
  line still opens that line's own draft. The panes' text-selection changes
  refresh Comment's text once a turn, because a drag selects lines without
  changing the selected line.
- **Docs.** The workbench specification's line-cursor, Return and right-click
  paragraphs describe the number gestures, the empty-line rule, Alt+C on
  several lines and the one-pane menu.

The shared files touched are `diff_workbench.cpp`/`.hpp` and
`workbench_window.cpp`. Review submission, Finish review, the composed
application and `src/app` are untouched.

## Acceptance

- **AC1.** One pane has the code's menu, on its code and its numbers, for the
  line each row shows.
- **AC2.** Line numbers select as GitHub's do, in both views; the "+" and the
  number's right-click keep working.
- **AC3.** A drag ending on an empty line takes it.
- **AC4.** Comment on several lines starts one draft on all of them.
- **AC5.** Related suites, gates and docs pass.

## Verification

macOS (Apple silicon), 2026-10-21, the `dev` preset's Debug configuration
built with `-g0` and the main checkout's CPM cache. Every case runs on the
offscreen platform and sends its pointer and key events to the window through
`QWindowSystemInterface`, with the event loop turning between events, so Qt's
own hit-testing picks the code, the gutter, the "+" or the popup.

- **Failing before.** With main's `diff_workbench.cpp`/`.hpp` and
  `workbench_window.cpp` swapped in and the new cases run one at a time, each
  fails on its own gap: one pane, "a right-click in one pane opens the code's
  menu on the clicked head line"; numbers, "split: a click on a line's number
  selects that line"; empty line, "split: a drag from line 5 that ends on the
  empty line 8 selects lines 5 to 8"; Comment, "Comment names the selected
  lines: Co&mment on line 3…", and with that check set aside, "Option-C on
  lines 3 to 5 opens one draft on all three". The product sources were then
  restored.
- **Suites pass:** `qt_workbench_line_draft` (the four new cases, BPR-1721's
  `test_a_selection_takes_a_range_comment_from_its_menu` and BPR-1682's
  `test_the_plus_on_a_pull_request_without_threads_opens_its_draft`),
  `qt_workbench_conversation_reveal` (BPR-1698's
  `test_a_pick_into_a_long_unopened_file_reveals_its_line`),
  `qt_conversation_cards_view`, `qt_diff_workbench_scale_100`, `_125` and
  `_200`, `qt_workbench_window_scale_100`, `qt_composed_github_scale_100`,
  `qt_composed_inline_drawer_scale_100`, `qt_composed_long_thread_scale_100`,
  `application_line_draft_contract`, `application_label_authoring_contract`,
  `application_diff_workbench_public_path`,
  `application_diff_workbench_shutdown_public_path`,
  `command_correctness_registry_contract` and
  `application_review_round_trip_composition_contract`: 16 of 16.
- **Gates.** `coding-format-check` (clang-format 22.1.8) passes on 1,731
  files. clang-tidy (LLVM 22) with warnings as errors passes on
  `diff_workbench.cpp` and `workbench_window.cpp`. On
  `workbench_window_tests.cpp` it reports only the two existing `std::endl`
  uses in the case tracer, which this change does not touch.
  The command-correctness reseal re-pins `workbench_window_tests.cpp` and
  adds the four cases to the `--line-draft` inventory. The round-trip
  composition pins are unchanged, since nothing in `src/app` changed.
  `verify_documentation_tests.py` passes 67 of 67. `verify_documentation.py`,
  run with `.agents` linked, refuses only this card's provisional ID format.
- **Owner look.** None asked. The criteria are gestures with exact,
  event-driven evidence, and nothing new is drawn: the selected lines wear
  the pane's existing text selection, and the menu and Comment use the
  wording BPR-1721's accepted range items already use.
