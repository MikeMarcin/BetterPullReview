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
