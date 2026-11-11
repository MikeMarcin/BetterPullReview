---
schema: "implement-rfc.card.v1"
id: "BPR-2094"
title: "Finish your review notes drafts that are not in the review"
status: "In Progress"
rfcs: ["RFC-BPR-0085"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1345","BPR-1590","BPR-2092"]}
depends_on: []
---

# Finish your review notes drafts that are not in the review

## Origin

The owner, 2026-11-11: "I also think that finish review should note draft
comments that haven't been added to the review."

## Scope and acceptance

1. Finish your review shows one line when any of my drafts is not in the
   review: "N drafts aren't in this review", and under it each draft with its
   file and lines. The line is absent when every draft is in.
2. **Add all** adds every draft a review can take; each row has **Add** (or
   says why a review cannot take it, e.g. a reply) and goes to its draft on a
   click. Nothing is added unless asked; a draft left out stays saved and is
   not published by Submit.
3. Which drafts a review can take, and what Add marks, is the core's
   decision (BPR-2092's place); the host performs it in one durable write.
4. The verdict buttons stay as they are: the reviewer can submit without
   adding them.
5. Owner look: Accept or Reject the line.

Load frontend-design before the Qt work.

## Implementation (2026-11-11)

Frontend-design was loaded before the Qt work.

- Finish your review (sheet and page) has a second section in its box under
  the pending comments: "N drafts aren't in this review", **Add all**, and each
  draft as a pending row is drawn (file, lines, label mark, first line), with
  **Add** where a review can take it, or why not under it ("A reply is sent to
  its thread on its own, never in a review."; "Open it to add it: it was saved
  before drafts recorded where they go."; "A review can't take it at its line:
  it was written on another diff or head."). A row's click goes to the draft.
  The section is absent when every draft is in, and while a submitted review
  waits. The verdict buttons are unchanged: Submit sends only what the review
  carries; a draft left out stays saved.
- A pending row's menu adds **Leave Out of Review**: the draft leaves the
  review and stays saved (a visible button would have narrowed every row of
  the 540-pixel sheet).
- Add and Add all: the core's `plan_add_to_review` (an edit at the listed
  revision that sets the place's mark), one durable write
  (`change_review_drafts`); Leave out: an edit that clears the mark.
- `qt_review_submission_editor_scale_{100,125,200}`; composed path
  `--attended-drafts-look` with `BPR_ATTENDED_DRAFTS_CHECK=1` on
  conventional-comments-v1: ATTENDED_DRAFTS_THREADS:5,2,
  ATTENDED_DRAFTS_OVERVIEW:3,2, ATTENDED_DRAFTS_OUTSIDE:2,
  ATTENDED_DRAFTS_ADDED:5.
