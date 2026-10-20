---
schema: "implement-rfc.card.v1"
id: "BPR-1665"
title: "Upgrade the pinned Qt runtime to 6.12.0"
status: "In Progress"
rfcs: ["RFC-BPR-0088"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"qt-6-12-v1","path":"docs/rfcs/plans/qt-6-12-v1/milestone-plan.json"},"mandate_revision":"qt-6-12-r1","assigned_implementer":"/root/qt612","reservation":"docs/issues/_reservations/BPR-1665--BPR-1665--78dea174f1c7.json"}
---

# BPR-1665: Upgrade the pinned Qt runtime to 6.12.0

Implement [RFC-BPR-0088](../../rfcs/RFC-BPR-0088-qt-6-12.md) in one bounded
milestone. No release publication or push. Native platform and release gaps
remain explicit. The verification ledger binds the candidate and actual checks.

## Acceptance

- AC1: Satisfy RFC AC01, preserving acquisition/version/module refusal gates.
- AC2: Satisfy RFC AC02 with actual source/ICU download hashes and focused package/source/security checks.
- AC3: Satisfy RFC AC03 through the available macOS app and composed offscreen checks using Qt6.12.0; retain unavailable native evidence.
- AC4: Satisfy RFC AC04, documentation checks and a durable command-quiescent checkpoint.
