---
schema: "implement-rfc.card.v1"
id: "BPR-1205"
title: "A tab over an older pair hides later threads until Show them"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1132"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner decision on attended candidate BPR-1132-later, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1132-later", "decision": "2c856168-4e06-4665-bc00-31fec66af8be", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-07"], "related_cards": ["BPR-1132", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1205: A tab over an older pair hides later threads until Show them

## Owner decision

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1132-later` on
the frozen m2 app (`ba9d2723`), decision
`2c856168-4e06-4665-bc00-31fec66af8be`, sequence 45, `actor: human`, the owner
selected, verbatim, "Hide later threads until Show them". No note. The built
tray keeps later threads listed but unplaced under the older-pair notice.

## Acceptance

- AC1 On a tab over an older pair, the Comments tray lists only the threads
  that belong to the tab's revisions. Threads on later updates are not listed;
  the notice names how many there are, and Show them reaches them through the
  existing routes.
- AC2 `qt_document_tabs_scale_{100,125,200}` and the tray and conversation
  courses pin it. A control that lists the later threads fails. A current-head
  tab still shows every thread and no notice.

No new owner question is needed for hiding itself; the owner's macOS look at
the built chrome under [[BPR-1135]] covers it.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering evidence — 2026-09-26

Decision: filter complete current-head thread roots and their descendants from
both tray views while both endpoints are older. Keep the unfiltered forest
for Discuss and for a retained per-file notice-count index. Historical
unplaced entries retain their explicit unavailable wording; no old numeric
line is guessed onto a revision. The notice says Comparing rather than
claiming no historical threads exist. Show them's three routes are unchanged.

The conversation course and document-tab courses at 100/125/200 pass 4/4.
They assert absent later roots and replies, retained notice counts, current-
head restoration, all three Show them routes, and a negative control whose
current-head forest fails the older-pair exclusion predicate. Existing
BPR-0972 per-file indexes remain; the notice now uses a cached keyed count
instead of scanning the forest. Initial compilation exposed an incorrect
scalar assumption for file_identity; the corrected typed-key map builds.

No new human verdict is inferred. The owner already chose hiding; the
combined chrome look and originating platform/station debt remain with
BPR-1135. This evidence is scoped to Mac engineering.
