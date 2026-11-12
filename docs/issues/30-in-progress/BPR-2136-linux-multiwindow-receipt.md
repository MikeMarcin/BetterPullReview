---
schema: "implement-rfc.card.v1"
id: "BPR-2136"
title: "Preserve accepted PR17 receipt and notes after a multi-window replacement"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2136--BPR-2136--83c31710051c.json","source_baseline":"6efba00e1afbf874c88c7e5de2623a33528ff36f","mandate_revision":"linux60-multiwindow-receipt-r1","milestone_plan":{"id":"bpr-2136-linux-multiwindow-receipt","path":"docs/rfcs/plans/bpr-2136-linux-multiwindow-receipt/milestone-plan.json"}}
---

# Diagnose the late receipt isolation failure

Linux60 Release passes this public course in10.0097s. Debug exits1 at11.20s after its earlier own-receipt ordering and conversation fanout checks pass. The final composite assertion requires the new comparison to name PR17, its receipt to be Read, and its overview to retain exactly two private notes while late PR18 fanout is pending. The failed conjunct is not yet captured. No original failure or product cause is cleared by this diagnosis.

## Acceptance

- AC1: Identify the exact failed PR17 receipt/private-note conjunct and its accepted/stale authority, retaining the original Linux60 Debug failure and actual Release counterpart. Prove the cause through one serialized bounded native diagnostic or deterministic targeted control before choosing a fixture or production correction.
- AC2: Implement the smallest proven correction without relaxing any original receipt, material, PR, private-note or late-delivery assertion. Preserve bounded waits/outer watchdog and public gestures; a fixture-only synchronization repair must have a meaningful controlled negative.
- AC3: Run focused exact-candidate public-path and affected closure checks through the shared native lease, preserve original failures/unreached checks, pass documentation/format/source controls, and land only own reviewed commits on canonical main with exact mapping for selective release intake.
