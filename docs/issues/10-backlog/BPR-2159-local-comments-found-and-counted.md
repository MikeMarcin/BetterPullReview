---
schema: "implement-rfc.card.v1"
id: "BPR-2159"
title: "Threads' state filter, Overview's Local chip, My review and Finish your review"
status: "Backlog"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2158"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC07","AC08","AC09"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2159: Threads' state filter, Overview's Local chip, My review and Finish your review

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `finding-and-counting` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. Threads: My drafts becomes the All, Published, Draft, Local state filter with counts (not remembered; Draft carries the amber dot); Local rows in their files' groups; Find a thread searches Local. Overview: the Local chip beside the drafts chip, a folder's sum, activation opens Threads on Local at the file's first; the Note column and Private notes row go; My review counts comments by state. Finish your review: Local never pending or not in the review; Keep Local on a not-in-review row; one quiet "N local comments stay on this computer" line with Show, absent at zero. Insights' Private notes tile is renamed Local comments (Q13).

## Acceptance

- **AC07:** Threads lists Local comments in their files' groups and its state filter's four choices show exactly the rows of that state with matching counts; the filter is not restored on reopening; under Local, Hide resolved and Only new hide no Local comment.
- **AC08:** A file's Comments cell on Overview shows a Local chip with its count beside the drafts chip, a folder shows the sum, the chip's activation opens Threads on Local at that file's first, and Overview has no Note column or Private notes row.
- **AC09:** Finish your review never lists a Local comment as pending or as not in the review; Keep Local on a not-in-review row removes that row; the Local line shows the count and is absent at zero.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build whose fixture holds Local, Draft and Published comments.
