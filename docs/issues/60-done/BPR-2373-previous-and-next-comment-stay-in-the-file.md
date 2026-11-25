---
schema: "implement-rfc.card.v1"
id: "BPR-2373"
title: "Previous and Next comment stay in the file and stop at its ends"
status: "Done"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2373--BPR-2373--e59ab9aed5b8.json","origin":"owner, 2026-11-24","related_cards":["BPR-2158","BPR-2262"]}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"26daa15a3","evidence":{"kind":"command","command":"ctest --preset dev -R '^core_comment_steps$'","detail":"core::decide_comment_steps: shown lines top to bottom then the rest by line, ties by projection order then input order; at the first no Previous, at the last no Next; one comment open has neither; none open enters at the ends; no comments has neither; a filtered-out comment is no stop"}},{"criterion":"AC-b","outcome":"pass","revision":"26daa15a3","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_next_comment_visits_drafts_and_keeps_their_words: at the first file's last comment Next is disabled and, forced on and triggered, leaves the comment open and the file shown; Hide resolved makes the draft at line 27 the last; at the first comment Previous is disabled and does nothing; in the second file, entered with none open, Previous at its first comment does not go back to the first file and Next at its last does not wrap; tooltips start 'Previous comment in this file' and 'Next comment in this file'"}},{"criterion":"AC-c","outcome":"pass","revision":"26daa15a3","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_current_file_unread_badge$'","detail":"test_the_right_tray_holds_comments_and_notes_tabs: Previous disabled at the first comment, Next at the last; both offered with none open; Only new with nothing new leaves no cards and both disabled; both offered again once listed"}}]
---

# Previous and Next comment stay in the file and stop at its ends

## Origin

The owner, 2026-11-24: "The next and prev comment buttons in the comments
drawer appear to be going over all the comments in the whole PR. They should
just go through comments in the current file and stop when it reaches either
end."

## Scope and acceptance

- **AC-a.** The core decides the order of the file's comments (published
  threads, my drafts and Local comments, as Comments lists them after its
  filters) and the step each way from the one open. At the first there is no
  Previous, at the last no Next; there is no wrap and no other file. With none
  of the file's comments open, Next is the first and Previous the last; with
  none listed, neither.
- **AC-b.** The comments drawer's Previous and Next comment buttons, their
  actions and their keys (Option-[ and Option-]) follow that decision: the
  step at an end is disabled and does nothing, and the enabled state follows
  the file shown, the drawer's filters, the comment open and comments added
  or removed. Their tooltips say "Previous comment in this file" and "Next
  comment in this file".
- **AC-c.** Qt tests show the enabled states and that a step at the end
  leaves the open comment and the file shown unchanged.

## Root cause

`workbench_window::step_comment` (BPR-2158, extended by BPR-2262) stepped
through the drawer's cards and, past either end, walked
`conversation_cards_view::comments_in_file_order` (every thread and draft of
the pull request in path order) into the next or previous file, wrapping at
the pull request's ends. `update_comment_steps` enabled both actions whenever
the pull request had any comment, so neither was ever disabled at an end. In
the whole-pull-request scope the drawer's cards span every file, so even the
within-file stepping crossed files there. The cross-file walk was the
implementer's reading in BPR-2158 ("then into the previous or next file's"),
not an owner request; the owner accepted the v003 look of it
(`previous-next-comment`, session `acaf71a2…`) and now asks for the file only.

## Built

- `core::decide_comment_steps` (`src/core/include/bpr/core/comment_steps.hpp`):
  the order (lines shown in the comparison top to bottom, then the rest by
  line, ties by projection order then input order) and the optional step
  each way, without wrap.
- `workbench_window::file_comment_steps` gives it the cards Comments lists on
  the file shown (every card beside one file; only the shown file's over the
  whole pull request), after the drawer's filters.
- `update_comment_steps` enables each action from that decision and sets the
  tooltips; it runs whenever the cards' marks change (relayout, open card,
  filters, drafts, file).
- `step_comment` opens the decided comment as before (thread, saved draft,
  Local comment or the draft being written) and does nothing without one.
- `threads_in_file_order`, `comments_in_file_order` and `comment_stop`, used
  only by the cross-file walk, are removed.
- The workbench specification records the behaviour.

PR-wide comment navigation remains in Next new comment (Option-N and its
menu item), which still walks unread threads across the pull request, and in
the Threads list / Overview Comments.

## Verification

dev preset, Debug, macOS, offscreen, at `26daa15a3` (lane commit; 26daa15a3
on main to be filled at integration). Passed: `core_comment_steps`,
`qt_workbench_line_draft`, `qt_current_file_unread_badge`,
`qt_workbench_conversation_reveal`; before the last refactor of
`step_comment` (no behaviour change, re-run of the first three after it):
`qt_workbench_window_scale_100`, `qt_keyboard_shortcuts_scale_100`,
`qt_thread_moves_scale_100`, `architecture_boundaries`,
`architecture_boundary_contract`. Negative control: with Next enabled
whenever the file has comments, `qt_workbench_line_draft` fails at "at the
file's last comment Next is unavailable and Previous is not" and
`qt_current_file_unread_badge` at "at the file's last comment Next is
unavailable". `coding-format-check` passes; clang-tidy on the touched
translation units reports only the existing `row_rect` naming warning in
`conversation_cards_view.cpp` (not touched by this change). No fixture
public-path course was added.
