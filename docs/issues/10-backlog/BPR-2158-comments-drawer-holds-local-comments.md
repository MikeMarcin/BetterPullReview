---
schema: "implement-rfc.card.v1"
id: "BPR-2158"
title: "Comments absorbs Notes: Local cards, the state control, N and one gutter mark"
status: "Backlog"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2157"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC01","AC02","AC04","AC05","AC06","AC14"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2158: Comments absorbs Notes: Local cards, the state control, N and one gutter mark

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `comments-absorb-notes` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first; the accepted design is [docs/design/comments-absorb-notes-v1](../../design/comments-absorb-notes-v1/README.md). The Notes tab goes (right drawer and Overview drawer); Local comments are cards in Comments at their lines, replies inside their threads and edge chips for the pull request, whole files and other files; the composer's Local/Draft (Local/Reply) state control in the foot row; N opens it on Local; the computer icon with the tooltip "Only on this computer" (owner, Q1) marks Local and a draft stops wearing the lock; order in a card and a line's group; one gutter mark a line with accessible text naming everything on it; Make draft on line N for an anchor the comparison does not place.

## Acceptance

- **AC01:** The right drawer's tabs are Comments, Tour and Agent, and the Overview drawer has no Notes tab; N on a selected line opens the composer on Local in Comments.
- **AC02:** On a line that has a published thread, a Local reply and a draft reply of mine, one card shows all three in the order defined, each with its state's mark, and the line has one gutter mark whose accessible text names all three.
- **AC04:** Keep local on a draft in the review lowers the review's pending count by one, removes it from every "not in this review" list, and keeps its text, line or range and reply target; Make draft on it returns a draft of the same kind at the same place, not in the review.
- **AC05:** Keep local on a draft held by a submitted review or being published, and Make draft where my draft already stands, are refused with their reasons and change nothing; no action offers a state change on a published comment.
- **AC06:** A Local comment on the pull request, on a whole file, on another file, and on a line the shown comparison does not place each appears where "Where a Local comment can be" says, the last with its reason; none is shown at a line it was not written on, and Make draft on it is offered only as Make draft on line N with a line selected.
- **AC14:** Each state is announced in words to assistive technology ("Local comment, only on this computer", "Draft, not in your review", "Draft, in your review"), and the state control, Make draft and Keep local are reachable and operable by keyboard alone.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build whose fixture holds Local, Draft and Published comments on one line.
