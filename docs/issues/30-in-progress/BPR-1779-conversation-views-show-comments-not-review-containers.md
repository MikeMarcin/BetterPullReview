---
schema: "implement-rfc.card.v1"
id: "BPR-1779"
title: "Conversation views show the comments themselves; a review is only the bundle they were posted in"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1779--BPR-1779--f70667adfb36.json","kind":"feature","found_by":"owner photograph and chat, 2026-10-26/27: GameEngine/game-engine #186, Pull request view","related_cards":["BPR-1667","BPR-1581","BPR-1586"]}
acceptance: []
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
