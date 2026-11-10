---
schema: "implement-rfc.card.v1"
id: "BPR-1981"
title: "Preserve published repaired sources and mixed comment namespaces during maintenance"
status: "Ready"
rfcs: ["RFC-BPR-0041", "RFC-BPR-0042"]
depends_on: ["BPR-1979"]
priority: "P2"
metadata: {"kind":"bug","related_cards":["BPR-1979"],"mandate_revision":"tour-destination-repairs-r1-maintenance-corrections","milestone_plan":{"id":"tour-maintenance-corrections-v1","path":"docs/rfcs/plans/tour-maintenance-corrections-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1981--BPR-1981--5ced895e4cc7.json"}
---

# BPR-1981: Preserve published tour maintenance identities

The owner authorized correcting both P2 critical-review findings after BPR-1979
completed. BPR-1979's original acceptance and evidence remain historical. Baseline
`69f745a9914428046f030a88617dbe68b7f2ca99`.

- Repaired stops retain an author-selected mapping after initial publication;
  a second Preview/Publish mistakes them for pending retargets and verifies the
  obsolete original subject.
- Mixed maintenance combines review and conversation comments, then chooses by
  numeric ID alone. Valid overlapping IDs produce a false conflict.

## Acceptance

- AC1. Initial repaired publication retains the confirmed published source,
  supports no-op preview and actual subsequent text maintenance, and preserves
  source bytes, stable remote identities and ordinary retarget behavior.
- AC2. Production-session maintenance supports overlapping review/conversation
  numeric IDs in stops and overview lookup; wrong node, URL, actor, kind or source
  still refuses without writes. File subjects remain distinct from conversation.
- AC3. Meaningful permanent regressions and focused/static/documentation checks
  pass for the exact candidate; correction evidence and command state are durable.
