---
schema: "implement-rfc.card.v1"
id: "BPR-0648"
title: "Aligned comment cards in the Read drawer"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"RFC-BPR-0028 AC-05 for Read: this file's threads aligned to their lines, cards that never overlap and follow the comparison's scroll, drafts","outcome":"pass","revision":"d04a15c1 + this finish","evidence":{"kind":"observation","detail":"qt_conversation_cards_view: cards sit at the y their locator gives for their line in line order, a card whose line is under the card above sits beneath it (no two card rects intersect), the unplaceable thread follows the placed ones, relaying against moved lines moves the cards, Whole PR lists every thread in file then line order, a press reports the thread and an action press reports Comment here, the shown draft is a dashed card at its one-based line with Publish, Edit and Delete. qt_composed_github_scale_100: the cards sit in the comments drawer with This file chosen, Whole PR lists every thread, Comment here makes the thread current and focuses the draft editor. The window relays the diff's line_rect through its locator and relays every applied scroll frame (diff_workbench::set_frame_observer) to the cards' layout, and paints amber thread marks in the gutter from marked_lines. Unread state, Mark read, Resolve and threaded Reply have no authority and are carded as BPR-0656 and BPR-0657; the card's action is Comment here."}},{"criterion":"AC-08: the twenty-post, three-branch recorded conversation reads correctly","outcome":"pass","revision":"d04a15c1 + this finish","evidence":{"kind":"observation","detail":"No recorded conversation exceeded nine posts; tests/fixtures/github/conversations/branching-thread-v1 is a constructed twenty-four-post thread with three branches and a fork, in deep-thread-v1's schema. qt_conversation_cards_view reads it and pins one card with all twenty-four posts in parent-before-child order, every branch's depth from the root, posts stacked on the rail, the card scrolling within the view and the thread's line marked. Discuss reads the same fixture under BPR-0649."}},{"criterion":"Focused delegate and drawer tests; the round-trip composition contract stays green with its digests repinned","outcome":"pass","revision":"d04a15c1 + this finish","evidence":{"kind":"observation","detail":"qt_conversation_cards_view (new), qt_github_conversation_panel, qt_github_thread_map_scale_100, qt_diff_workbench_scale_100, qt_composed_github_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, qt_review_draft_editor, github_conversation_fixture_contract, the public-path courses, application_workbench_composition_contract, application_review_ledger_composition_contract, application_uiforetw_selected_diff_contract, application_composition_contract and application_review_round_trip_composition_contract (digests repinned in two passes) pass; check_boundaries, strict tidy on every touched translation unit (two pre-existing local names in diff_workbench.cpp fixed in passing), clang-format --Werror and verify_documentation.py pass."}}]
---

# Aligned comment cards in the Read drawer

RFC-BPR-0028 mockup state 2. The comments drawer shows this file's threads as
cards positioned at their lines (root anchored to the line like Code
Collaborator), replies on an avatar rail with a thread line, unread dots and
read-state dimming, Reply, Resolve and Mark read, and my unpublished notes as
dashed drafts with Publish, Edit and Delete. Gutter marks in the diff: amber
for a thread, blue for a new one; the strip's jump control walks the blue
ones. Whole PR is a filter listing every thread in file order. Uses the
existing conversation model and delegate; no new authority.

## Acceptance

- RFC-BPR-0028 AC-05 for Read, including cards that never overlap and follow
  the comparison's scroll.
- The twenty-post, three-branch recorded conversation (AC-08) reads correctly.
- Focused delegate and drawer tests; the round-trip composition contract stays
  green with its digests repinned.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- What exists and what does not, found before the first edit: the
  conversation model, delegate, panel and thread map present a forest of
  `github_conversation_projection` nodes with no read state, no draft and
  no per-line geometry; the diff surface exposed one anchor marker and no
  way to ask where a line is; per-post read state has no authority beyond
  `core::conversation_receipt` and an unplumbed SQLite table; Reply and
  Resolve have no intent or port method (resolve is a capability only);
  drafts (`core::review_comment_draft`) carry a line anchor and live in the
  mutation host; and no recorded conversation had more than nine posts.
  The card's "no new authority" therefore bounds this slice to the
  presentation, and the two authorities the design needs are carded:
  [BPR-0656](../10-backlog/BPR-0656-post-read-state-through-conversation-receipts.md)
  (post read state: unread dots, dimming, Mark read, blue marks, the
  strip's next-new-comment group) and
  [BPR-0657](../10-backlog/BPR-0657-threaded-replies-and-resolve-through-the-mutation-port.md)
  (threaded replies and resolve). Until they land no post is drawn read or
  unread, every thread mark is amber, and a card's one action is "Comment
  here", which starts a draft on the thread's line rather than claiming to
  reply into the thread.
- `conversation_cards_view` (`src/adapters/qt`): a drawn scroll area, as the
  thread map is, over the panel's retained forest. One card per review
  thread: the replies in projection order on an avatar rail with a thread
  line, my posts' avatars in the accent (the viewer login comes from the
  session projection), Resolved and Outdated tags, and the action row. In
  the This file scope a card sits at its line, asked of a locator the
  window supplies, or beneath the card above when the lines are close, so
  cards never overlap; unplaceable threads (outdated, no current line)
  follow the placed ones. Whole PR lists every thread in file then line
  order under its path. My shown draft, while it is on the current
  comparison, is a dashed card at its line with Publish, Edit and Delete.
  A press makes a thread current and reports its root; Up, Down and Return
  work the same from the keyboard; a press on an action reports it.
  `marked_lines(side)` gives the gutter its marks. Pinned by
  `qt_conversation_cards_view`.
- The diff surface (`diff_workbench`) gained `line_rect(side, line)` (one
  pane row in the workbench's coordinates), `set_line_marks(side, marks)`
  painted at the pane's edge under the anchor marker, and
  `set_frame_observer`, told after every applied scroll frame. The window
  relays the geometry: its locator maps a line's rect from the comparison to
  the cards' viewport, and its frame observer relays the cards' layout, so
  the cards follow the shared scroll without the drawer reading the panes.
- The panel places the cards pane first in the loaded splitter, above the
  map and the tree, with This file and Whole PR as exclusive buttons at its
  head; the tree stays the selection's owner: a card press moves the tree's
  current index, which reveals the line as before, and the tree's current
  index names the card. The window refreshes the cards (draft, layout,
  marks) on adoption, on every anchor-file change, on every frame and on
  clear; Comment here makes the thread current and focuses the draft
  editor; Publish, Edit and Delete drive the editor's own actions.
- AC-08's conversation did not exist: `tests/fixtures/github/conversations/branching-thread-v1`
  is a constructed twenty-four-post thread with three branches off the
  first reply and a fork inside one branch, in `deep-thread-v1`'s schema.
  The cards test reads it and pins one card with every post in
  parent-before-child order and every branch's depth; Discuss (BPR-0649)
  reads the same fixture.

