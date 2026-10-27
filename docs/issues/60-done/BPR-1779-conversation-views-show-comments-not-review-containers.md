---
schema: "implement-rfc.card.v1"
id: "BPR-1779"
title: "Conversation views show the comments themselves; a review is only the bundle they were posted in"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1779--BPR-1779--f70667adfb36.json","kind":"feature","found_by":"owner photograph and chat, 2026-10-26/27: GameEngine/game-engine #186, Pull request view","related_cards":["BPR-1667","BPR-1581","BPR-1586"]}
acceptance: [{"criterion": "AC1 fixture renders", "outcome": "pass", "revision": "894558239a81471db3a6d61f8b6082ed06125057", "evidence": {"kind": "observation", "detail": "Offscreen renders of #186's shape (tests/adapters_qt/owner_186_fixture.hpp: a no-body Commented review with two file comments, two body-less Commented reviews that carried replies, two approvals) in the drawer's Pull request view, Discuss over the whole pull request and Threads, closed and with a comment open, light and dark, before (at f71003dad) and after. conversation_cards_view_tests --render-bpr-1779 <dir> <phase> and conversation_finder_tests --render-bpr-1779 <dir> <phase>. Kept outside the repository; forced-style offscreen renders are layout evidence only."}}, {"criterion": "AC2 Qt tests", "outcome": "pass", "revision": "894558239a81471db3a6d61f8b6082ed06125057", "evidence": {"kind": "command", "detail": "qt_conversation_cards_view test_conversation_lists_comments_not_reviews (Pull request view and Discuss): the no-body review's two file comments are the cards, with path and line lines that open them in Review, the open comment's excerpt, Open in Review, its reply, Reply and Resolve dispatched to the thread and opening reading the thread; body-less Commented reviews add no row; approvals are compact verdict lines; no empty-review notice; Unread and Hide resolved act on comments and drop verdict lines; the chip counts 2 comments, not 6 reviews; File scope keeps line order without review lines. test_a_review_body_is_a_comment_in_its_bundle; test_pull_request_comments_live_at_the_edge updated. qt_conversation_finder test_reviews_are_rows_only_for_a_body_or_a_verdict. core_conversation_timeline for the grouping, visibility, order, filter and count rules. Adjacent: qt_workbench_window_scale_100, qt_composed_markdown_1_0/1_25/2_0, qt_discuss_comment_bodies (updated for the bundle), qt_github_conversation_panel, qt_drawer_surfaces_fit_scale_100, qt_conversation_finder_scale_100/125/200, application_discuss_top_public_path, application_conversation_read_public_path, architecture_boundaries, architecture_boundary_contract pass."}}, {"criterion": "AC3 owner look", "outcome": "pending", "revision": "894558239a81471db3a6d61f8b6082ed06125057", "evidence": {"kind": "observation", "detail": "Deferred by the owner, not a blocker. Questions for the look on a frozen build: (1) the grouping form chosen, one quiet line per bundle (avatar, who, verdict word when Approved or Changes requested, date) with a rail beside its comments, versus attribution on each comment; (2) verdict-only rows as compact lines."}}, {"criterion": "AC3", "outcome": "pass", "revision": "aaa24c11c865068cc24b918dd9e4e7f7d61b2c99", "evidence": {"kind": "observation", "detail": "Attended review session 41f51349bc37e6c092d31dbf1739c5b1 (state ~/.attended-review-state/bpr-looks-2026-10-27, served over Tailscale, handed back 2026-10-27T21:11Z): candidate BPR-1779-grouping and BPR-1779-verdict-rows, owner verdict accept."}}]
---

# BPR-1779: Conversation views show the comments themselves; a review is only the bundle they were posted in

## Observed

The owner's photograph (a pull request in the drawer's **Pull request**
view, dark): a card "Pull request Commented" for dosullivan whose body reads
"No overall review comment. 2 file comments are linked below.", then two
path buttons (`Client/AppCore/src/Instance.cpp`,
`Client/AppCore/include/AppCore/Instance.h`) and Reply; below it, rows that
say only "fzhu Commented", "dosullivan Commented", "cswiedler Approved",
"saustin Approved".

The owner, verbatim:

> I don't understand why discuss and threads aren't showing these comments
> directly

> The review object is honestly pretty unimportant to me. I'm not sure what
> the benefit of it is. Maybe it's a necessary evil for how we have to
> interact with the system, but to me the review is just a bundle of comments
> posted together and what really matters is the comments themselves on the
> files and lines

## Cause

The conversation views model GitHub's objects literally. A review (verdict
plus optional overall body) is one card; its inline comments are separate
threads, listed under **File** and beside the code in Review.
[[BPR-1667]] gave a review with no overall body the empty state "No overall
review comment. N file comments are linked below." with path links,
deliberately "without borrowing any body"
(`src/adapters/qt/src/conversation_open_card.cpp`, around line 1523;
`thread_card::ReviewComments`). Discuss and Threads build the same card. So a
review that is a bundle of file comments shows as links, and a review with
nothing but a "Commented" verdict is a row with no content.

## Outcome (direction from the owner; details for the look)

Comments are the unit; the review is attribution.

- **Pull request view, Discuss and Threads list comments.** Each inline
  comment thread appears in the conversation in its own right: path and line
  (a link that opens it in Review), a short excerpt of the lines it is on, the
  comment, its replies, and Reply. Pull-request comments appear as now.
- **A review is a light grouping, not a card.** Comments posted together in
  one review sit under one quiet line naming who posted them and when (and
  the verdict, when it is Approved or Changes requested), or show the same
  attribution on each comment; the look decides which.
- **A review with an overall body** shows that body as one more comment in
  the bundle.
- **A review with no body and only a "Commented" verdict adds no row of its
  own**: its comments carry it. "Commented" rows with nothing under them
  disappear.
- **Approved and Changes requested stay visible** (they matter for merging),
  as a compact verdict line with whoever gave it, and the review's comments
  under it. People (Overview) is unchanged.
- No comment's body is shown as another object's words: an inline thread is
  still its own thread with its own identity, read state, reply and resolve,
  so [[BPR-1667]]'s concern holds.
- Ordering: by time, as GitHub's Conversation tab; the File and Review views
  keep their file-and-line order.
- Unread, Unresolved and Hide resolved filters apply to the comments; counts
  count comments and threads, not reviews.
- UX work: load `frontend-design:frontend-design` first, and design within
  the drawer kit (RFC-BPR-0085).

## Acceptance (draft)

- Fixture renders of #186's shape (a no-body review with two file comments,
  "Commented" reviews with nothing, two approvals) in the Pull request view,
  Discuss and Threads, light and dark, before and after.
- Qt tests: a no-body review's file comments appear as threads in the
  conversation with path, line, excerpt and Reply; a body-less "Commented"
  review adds no row; an approval stays a compact line; read state, reply
  and resolve act on the comment's own thread; counts and filters count
  comments.
- The owner accepts the look on a frozen build (attended review: one
  question for the grouping, one for verdict-only rows).

## Resolution

Comments are the unit; the review is attribution ([[BPR-1779]]).

- Core: `core::arrange_conversation_timeline`
  (`src/core/include/bpr/core/conversation_timeline.hpp`) decides which
  reviews take a row, groups each review's comments (its body, then its
  threads in file and line order) under one bundle line, orders units by time
  (legacy items without a time fall back to file and line order), applies
  Unread, Hide resolved and drill-down filters to comments only, and counts
  comments, threads and unread comments, never reviews.
- Qt: the drawer's Pull request view and Discuss over the whole pull request
  (`conversation_cards_view::timeline()`) read in that order. Each thread a
  review carried is its own card under a path-and-line line that opens it in
  Review; open, it shows its excerpt when the compared file has it, its
  replies, Reply and Resolve, with its own identity and read state. A bundle's
  quiet line names who and when, and the verdict only when Approved or Changes
  requested, with a thin rail beside its comments. A body-less Commented
  review adds nothing; an approval that carried nothing is a compact verdict
  line. [[BPR-1667]]'s "N file comments are linked below" notice and its links
  are gone; a review card that still appears outside the time order says only
  "No overall review comment.", so no body is borrowed. The Pull request chip
  counts comments. File scope and Review keep file-and-line order.
- Threads keeps its file grouping; under Not on a file, reviews follow the
  same rule in time order, an approval's row saying "Approved".

Follow-up on read state: an unread body-less review had no card and next new
skipped it, yet the host still observed it as a post, so the Status "new
comments" line, the Comments tab count and the queue's unseen count counted a
comment nothing could open, and Mark all read stayed disabled when it was the
only one. `core::review_body_is_comment` now decides it once: a review whose
body is blank is never unread content, in the host's read observations (counts,
Mark all read, a single read) and in the cards' navigation alike.
`application_conversation_read_public_path` adds such a review and failed
before ("host count reaches Status": three new comments, not two);
`core_conversation_timeline` covers the rule.

The look (one line per bundle, verdict-only rows) is the owner's, pending.
