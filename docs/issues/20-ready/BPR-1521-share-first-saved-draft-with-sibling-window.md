---
schema: "implement-rfc.card.v1"
id: "BPR-1521"
title: "Share a first-saved inline draft with an untouched sibling window"
status: "Ready"
rfcs: []
depends_on: ["BPR-1520"]
priority: "P1"
metadata: {"source":"Coordinator combined testing-note integration regression","assigned_implementer":"/root/bpr1520","related_cards":["BPR-1520"],"baseline":"3c24b9ec834fcafc87af05680f9f2799708b4267","milestone_plan":{"id":"bpr-1521-sibling-first-save","path":"docs/rfcs/plans/bpr-1521-sibling-first-save/milestone-plan.json"}}
---

# BPR-1521: Share a first-saved inline draft with an untouched sibling window

The combined application_profile_multi_window_public_path check fails at “accepted draft edit reaches sibling editor”. BPR-1520 correctly made line-selection placeholders runtime-only, but the first saved edit receives a durable draft identity while an untouched sibling editor still holds its own placeholder identity. The existing shared publication subscription therefore ignores the durable record. This is a follow-up integration regression; keep BPR-1520’s historical acceptance unchanged.

## Acceptance

- AC1: Reproduce the composed sibling-editor failure on the integrated BPR-1520 baseline.
- AC2: After a first durable edit, an untouched sibling for the same complete authority, exact anchor and reply identity adopts the durable draft and displays the accepted text.
- AC3: Never overwrite an independently authored sibling candidate, pending save or different scope/anchor/reply. Preserve both authored candidates when reconciliation cannot safely merge them.
- AC4: Verify actual composed multiwindow propagation plus focused identity/save races, restart and existing BPR-1520 durability/recovery checks. Do not weaken the existing content assertions to accommodate lost sharing.

## Coordination

Correct this before declaring the combined testing-note batch accepted. The BPR-1520 implementer owns diagnosis and the bounded correction; main integration remains coordinator-owned. No owner profile, live GitHub review, desktop or browser data is inspected or changed. Follow existing layer boundaries and frontend-design requirements for any UX edits.
