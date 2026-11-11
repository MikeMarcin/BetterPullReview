---
schema: "implement-rfc.card.v1"
id: "BPR-2092"
title: "My drafts are found in Threads, labelled Draft, with a filter for only my drafts"
status: "Backlog"
rfcs: ["RFC-BPR-0085"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1293","BPR-1345","BPR-2093","BPR-2094"]}
depends_on: []
---

# My drafts are found in Threads, labelled Draft, with a filter for only my drafts

## Origin

The owner, 2026-11-11: "I don't seem to have an easy way to find all my draft
comments." On Home: "I dunno if I'd put it there. I feel like discuss or
threads is probably better." No Home change.

## How drafts work today

A comment written at a line is a private local draft (`review_comment_draft`,
state `drafting`) the moment it is saved. **Add review comment** / **Start a
review** gives it a pending mark (`InReview`: the file's path at the head):
it is then *in the review* and rides the review's Submit. A draft without the
mark is *not in the review*: it is published only by its own Publish. A reply
can never be in a review. The window shows a saved draft only at its line in
the file on screen, and Threads shows only the one draft open in the drawer.
A saved draft does not record its file, so a draft in a file not opened this
session cannot be listed by file.

## Scope and acceptance

1. Every saved draft records where it was written: the file and whether a
   review could take it at its own line (the pending mark Start a review would
   give it). Older drafts without one are placed when their comparison is
   shown. Stored drafts read unchanged.
2. The core decides each draft's standing (in the review, not in the review,
   held by a submission, a reply) and its place, with no Qt.
3. Threads lists every unsent draft of mine in its file's group, labelled
   **Draft**, and for one not in the review a distinct "not in review" mark.
4. Threads' filter bar has **My drafts**: only my drafts. Its choice is
   remembered with the other Threads filters.
5. A draft row's click (Return) goes to the draft's lines in Review with the
   draft open in the drawer.
6. Owner look on a frozen build whose fixture pull request has several drafts,
   some in and some not in the review, and existing threads: Accept or Reject.

Load frontend-design before the Qt work.
