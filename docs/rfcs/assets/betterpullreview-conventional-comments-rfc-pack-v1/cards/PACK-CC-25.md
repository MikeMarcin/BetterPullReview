---
schema: "implement-rfc.card.v1"
id: "PACK-CC-25"
title: "Verify and close label counters and settings"
status: "Backlog"
rfcs: ["RFC-BPR-0049"]
depends_on: ["PACK-CC-24"]
closeout_for: ["RFC-BPR-0049"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "closeout", "acceptance": ["R20", "R21", "R22", "R23", "R24", "R25", "R26", "R27", "R28", "R29", "R44", "R48"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-25: Verify and close label counters and settings

## Outcome and work

Reconcile Overview accounting, partial-state clarity, exact drill-down, settings and performance evidence. Check all selected-subset tests plus actual application behavior; follow canonical order.

## Acceptance

- Counters conserve the population for all label subsets and state scopes.
- Unknown/partial data, profile sharing and settings failure are presented honestly.
- Measured claims are bounded to actual targets/workloads and producing revisions.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
