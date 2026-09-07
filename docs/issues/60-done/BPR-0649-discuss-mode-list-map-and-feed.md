---
schema: "implement-rfc.card.v1"
id: "BPR-0649"
title: "Discuss mode: thread list, map and feed"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"RFC-BPR-0028 AC-05 for Discuss and AC-08","outcome":"pass","revision":"f4213732 + this finish","evidence":{"kind":"observation","detail":"qt_github_conversation_panel pins Discuss moving the tree and the map into the left column with the feed beside them, the Unread and Unresolved filters, the summary and the return to the Read arrangement; qt_conversation_cards_view pins the feed's file order, the author/reviewer headers and replying-to on the twenty-four-post three-branch fixture (AC-08), the excerpt from the compared file, Reply/Mark read/Open in code, and the Unread and Unresolved filters; qt_github_thread_map pins the unread fill from the ledger's state and its withdrawal; qt_workbench_window_scale_{100,125,200} pin Discuss widening the conversation to 640 and arranging the panel, the excerpt from the compared file, and the return to the persisted width."}},{"criterion":"Open in code returns to Read at the thread's line","outcome":"pass","revision":"f4213732 + this finish","evidence":{"kind":"observation","detail":"qt_workbench_window_scale_{100,125,200}: a press on the feed card's Open in code leaves Discuss for Read with the thread current in the conversation view, which is what reveals its line."}},{"criterion":"Focused tests for the list model, map selection and feed delegate","outcome":"pass","revision":"f4213732 + this finish","evidence":{"kind":"observation","detail":"The list is the drawer's tree (github_conversation_model and its property tests), map selection is pinned by qt_github_thread_map, and the feed by qt_conversation_cards_view; the composition contracts, the UIforETW courses, check_boundaries, strict tidy, clang-format --Werror and verify_documentation.py pass at the finish."}}]
---

# Discuss mode: thread list, map and feed

RFC-BPR-0028 mockup state 3. The Discuss modality widens the conversation
over the code: a thread list with file-and-line roots, replies under a guide
line, unread dots, counts, and Whole PR, This file, Unread and Unresolved
filters; the accepted thread map (`github_thread_map`) for the selected
thread with unread nodes filled; a feed with one card per thread carrying the
code excerpt on the product theme, the root and replies on a continuous rail,
per-post headers (author, role, replying to, unread dot, time) and Reply,
Resolve, Mark read and Open in code.

## Acceptance

- RFC-BPR-0028 AC-05 for Discuss and AC-08.
- Open in code returns to Read at the thread's line.
- Focused tests for the list model, map selection and feed delegate.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- Slice, 2026-09-07. Discuss is the conversation drawer widened, as the
  accepted mockup draws it (the conversation column widened, plain tree at
  the left): `github_conversation_panel::set_discuss` moves the same widgets
  into a horizontal arrangement, the tree above the map at the left and the
  feed at the right, with a summary ("n threads, u unresolved, k new")
  beneath the feed; the window enters it from the Discuss modality and
  leaves it with any other, and `central_sizes` grants the conversation at
  least 640 pixels while in Discuss without persisting that width. The
  feed is `conversation_cards_view` in feed mode: every thread as a card in
  file order, never placed at a line, the code at the thread's line (one
  line each side) on the theme's code background in the code font when the
  thread is on the compared file, each post headed by the author with
  "author" for the pull request's author and "reviewer" for anyone else
  and "replying to" whom a reply answers, the unread dot and full-ink name
  from BPR-0656, and Reply (the thread becomes current and the draft
  editor takes focus), Mark read (while unread) and Open in code (the file
  is selected through the files drawer's intent, Read is entered and the
  thread made current so its line is revealed). The Unread and Unresolved
  filters sit beside This file and Whole PR over the feed; the tree at the
  left stays the drawer's plain tree. The thread map fills the nodes the
  ledger calls unread (`set_unread`, the unread accent), and only those.
- Not in this slice: Resolve (the mutation port, BPR-0657); a post's time
  and finer role (the projection carries neither; author or reviewer is
  what the pull request's author login decides); filtering the tree at the
  left, which the window's selection paths address by identity; and
  "thread n of m" in the strip.
- AC-08: the twenty-four-post, three-branch fixture reads in the feed as
  one card in the projection's order with every reply's answer named
  (`qt_conversation_cards_view`), and in the drawer's tree and map as
  BPR-0648 and the map's own tests pin.
- Verification at the finish: `qt_conversation_cards_view`,
  `qt_github_conversation_panel`, `qt_github_thread_map_scale_{100,125,200}`,
  `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `qt_repositories_drawer`, `qt_settings_window`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (digests repinned),
  `application_review_ledger_composition_contract`,
  `application_queue_composition_contract`, the UIforETW course and
  selected-diff contract, `check_boundaries`, strict tidy on every touched
  translation unit, `clang-format --Werror` and `verify_documentation.py`
  pass.
