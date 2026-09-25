---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-02"
title: "Add provider-independent cycle identities and state transitions"
status: "Backlog"
rfcs: ["RFC-BPR-0053"]
depends_on: ["PACK-CYCLE-01"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-01", "RC53-02", "RC53-03", "RC53-09"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-02: Add provider-independent cycle identities and state transitions

## Outcome and bounded work

Introduce local_cycle as an explicit review subject, with local profile/workspace scope and strong IDs for source revisions, feedback drafts/rounds and verification. Implement finite core transitions and bounded projections. Existing PR authority types remain intact. Define unknown/partial scope and separate ordering/material equality.

## Acceptance and direct evidence

Core tests for no-PR R0, invalid/zero/cross-subject IDs, overflow, unrelated profiles, partial scope and absence versus empty. Assert bounded observations and no provider/Qt/effect access in the core.

Criteria: RC53-01, RC53-02, RC53-03, RC53-09.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
