---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-38"
title: "Add optional direct Send as a thin existing-profile adapter"
status: "Backlog"
rfcs: ["RFC-BPR-0060"]
depends_on: ["PACK-CYCLE-37", "PACK-CYCLE-27", "PACK-CYCLE-22"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-07", "RC60-08", "RC60-09"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-38: Add optional direct Send as a thin existing-profile adapter

## Outcome and bounded work

Deliver one exact frozen assignment using an explicitly selected admitted profile/session. Reuse provider process/credential/host policy and shared response correlation. Differentiate accepted, acknowledged, completed, failed and uncertain without making any state a capture gate.

## Acceptance and direct evidence

Payload parity with Copy/Save/CLI, existing-session versus explicit new-session disclosure, denied GHES profile, late wrong-round output, uncertain send, remove adapter mid-cycle and manual continuation; no automatic fallback/resend.

Criteria: RC60-07, RC60-08, RC60-09.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
