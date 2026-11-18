---
schema: "implement-rfc.card.v1"
id: "BPR-2262"
title: "Next and Previous comment visit my drafts and leave them their words"
status: "Done"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2262--BPR-2262--07158c3df77f.json","origin":"owner, 2026-11-18","related_cards":["BPR-2260","BPR-2158"]}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"53825f0d4","evidence":{"kind":"command","detail":"qt_workbench_line_draft test_next_comment_visits_drafts_and_keeps_their_words: from the draft at line 10 being written, Next opens the thread at line 22 and Previous the thread at line 5; each time the draft's note carries author reviewer and its words, not Writing"}},{"criterion":"AC-b","outcome":"pass","revision":"53825f0d4","evidence":{"kind":"command","detail":"same test, host answering lines and files a turn later: Next and Previous open the drafts at lines 10 and 27 of the first file in the drawer with their words; Next past the file opens the second file at its draft on line 8, then its thread; Previous from there opens the first file at its draft on line 27; both ends wrap to the other file's thread"}},{"criterion":"AC-c","outcome":"pass","revision":"53825f0d4","evidence":{"kind":"command","detail":"qt_conversation_cards_view test_a_draft_left_by_the_conversation_keeps_its_words: the conversation shown and hidden while the draft is written leaves it a note with author and words, no editor, and the note follows new words; the open card moves only through set_open"}},{"criterion":"AC-d","outcome":"pass","revision":"53825f0d4","evidence":{"kind":"command","detail":"on the unfixed source the window test fails at \"Next from my draft being written opens the thread after it (open review-thread:a)\" and the cards test at \"back on the lines, my draft is its note with my name and words, not an open composer\"; both pass with the fix"}}]
---

# Next and Previous comment visit my drafts and leave them their words

## Origin

The owner, 2026-11-18, after BPR-2260: "Probably the same issue but next
comment seems to also break the draft comment", and then: "I expect next
comment and prev comment to visit drafts".

## Scope and acceptance

- **AC-a.** From my draft being written in the drawer, Next and Previous
  comment go on to the comment after or before its line, and the draft
  keeps its note with my name and its words.
- **AC-b.** Next and Previous comment visit my drafts in file and line order
  alongside threads, within a file and across files, both ways, and landing
  on a draft opens it as clicking it does.
- **AC-c.** Every path that moves the drawer's open card off my draft ends
  writing it.
- **AC-d.** Qt tests fail without the fix and pass with it.

## Root cause

`workbench_window::step_comment` (Previous and Next comment, BPR-2158) built
the file's order from the drawer's cards without the draft being written.
From that draft the open card was not in the order, so a step started again
at the file's first comment (Next) or last (Previous): Next from a draft
between two threads went back to the first thread, the next Next landed on
the draft again, and nothing past it was reached. With the BPR-2260 fix not
yet in the owner's build, that step also left `Writing` set while another
card was open, which drew the draft as an empty frame.

Past the file, the walk was `threads_in_file_order`: published threads only,
so drafts in other files were never visited. BPR-2158 included drafts
within a file and did not set out to skip them in other files; the owner
now asks for them. A thread in another file was also only made current
(`go_to_conversation`), so its file did not open.

`conversation_cards_view` still moved `Open` without ending the draft in
two places: clearing the view, and the pull request's conversation shown
and hidden (`show_pull_request_comments`), which brought the draft back
open with `Writing` set and no composer begun by the window. `set_draft`
read the bare `Writing` flag.

## Built

- `conversation_cards_view::set_open` is the one way the open card changes,
  and ends writing the draft whenever the open card is anything else; it
  replaces `leave_draft` and every direct assignment. Leaving and returning
  to the lines does not reopen the draft. `set_draft` and `draft_editor`
  read `writing_draft()`.
- `conversation_cards_view::comments_in_file_order`: every review thread on
  a file and every draft of mine on a line (by its recorded path), in file
  and line order.
- `step_comment` keeps the draft being written in the file's order. Another
  line's saved draft opens through `reveal_pending_comment`, which chooses
  its line and opens it in the drawer once the host shows it; past the file
  a draft opens through `reveal_pending_comment` and a thread through
  `open_conversation_in_review`, each opening its file.

## Verification

On the lane, dev preset, macOS, offscreen. Both new tests fail on the
unfixed source and pass with the fix. Passed: `qt_conversation_cards_view`,
`qt_workbench_line_draft`, `qt_workbench_conversation_reveal`,
`qt_workbench_window_scale_100`, `qt_thread_moves_scale_100`,
`qt_compose_review_entry_scale_100`, `qt_keyboard_shortcuts_scale_{100,125,200}`,
`qt_composed_inline_drawer_scale_100`, `qt_composed_resident_drawer_filters`,
`qt_composed_github_scale_100`, `qt_local_draft_window`,
`qt_github_conversation_panel`. Format check passes on the changed files;
clang-tidy reports nothing on the changed lines. Not run: the whole suite.
