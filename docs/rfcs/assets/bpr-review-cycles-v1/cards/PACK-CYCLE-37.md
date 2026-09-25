---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-37"
title: "Package genuinely removable agent and automation components"
status: "Backlog"
rfcs: ["RFC-BPR-0060"]
depends_on: ["PACK-CYCLE-04", "PACK-CYCLE-25", "PACK-CYCLE-29"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-01", "RC60-02", "RC60-03"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-37: Package genuinely removable agent and automation components

## Outcome and bounded work

Finish build/composition splits and generate desktop/CLI packages with agent integrations absent; make local automation independently removable. Update resource/helper/install/SBOM manifests. Uninstall adapters without deleting any cycle data.

## Acceptance and direct evidence

Link/dependency closure and installed-file inspection, clean startup with no providers, manual two-round course, automation-off GUI course, publication preparation and re-open old cycles after removal.

Criteria: RC60-01, RC60-02, RC60-03.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
