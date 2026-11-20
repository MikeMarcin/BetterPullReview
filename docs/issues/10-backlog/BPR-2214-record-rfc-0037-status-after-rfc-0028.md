---
schema: "implement-rfc.card.v1"
id: "BPR-2214"
title: "Record RFC-BPR-0037 Auto-Verified once RFC-BPR-0028 closes"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1021"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2213--BPR-2214--32c543fd79d5.json","origin":"Hygiene audit 2026-11-15 gap on BPR-1194; carded under the owner's decision of 2026-11-16 on BPR-0653","related_rfcs":["RFC-BPR-0037","RFC-BPR-0028","RFC-BPR-0052","RFC-BPR-0061"],"related_cards":["BPR-0927","BPR-1021","BPR-1135","BPR-1194","BPR-0653"]}
---

# BPR-2214: Record RFC-BPR-0037 Auto-Verified once RFC-BPR-0028 closes

## Why

[RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md) is
Proposed. Its closeout [[BPR-0927]] is Done; the RFC waits only on
RFC-BPR-0028 being Auto-Verified, which [[BPR-1021]] closes. No card owned
the status change afterwards, and both [[BPR-1135]] (RFC-BPR-0052) and
[[BPR-1194]] (RFC-BPR-0061) wait on it. The 2026-11-15 hygiene audit found
the gap; the owner's 2026-11-16 decision on [[BPR-0653]] ("card what's left")
cards it. Since 2026-11-20 [[BPR-1194]] no longer waits: Review Cycles
were retired ([[BPR-2308]]), RFC-BPR-0061 closed to what shipped and dropped
RFC-BPR-0037 from `depends_on`.

## Acceptance

- AC1: With RFC-BPR-0028 Auto-Verified, RFC-BPR-0037's front matter and RFC
  index row read Auto-Verified, citing [[BPR-0927]]'s closeout evidence; no
  criterion is re-judged here. If RFC-BPR-0037 has an explicit dependency
  other than RFC-BPR-0028 that is not Auto-Verified, this card records it and
  stays open instead.
- AC2: `python3 scripts/verify_documentation.py --root .` and the board
  validator pass.
