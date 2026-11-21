---
schema: "implement-rfc.card.v1"
id: "BPR-2342"
title: "The large-review history course's epoch drain stalled once on the Ubuntu guest"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2339--BPR-2342--60dd1b567452.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2342: The large-review history course's epoch drain stalled once on the Ubuntu guest

## Finding

`application_large_review_history` stalled once waiting for its epoch work to
drain on the Ubuntu (Lima) guest. It is a timing course, judged on the WSL2 i9
(owner, 2026-11-18), where it passed every run in BPR-2215's rounds. Seen once;
not reproduced.

## Acceptance

- AC1. On a recurrence, the course names the epoch work still pending when its
  drain wait ends.
- AC2. Whether it is a stuck drain or a slow guest is decided from that, and a
  stuck drain is fixed.
