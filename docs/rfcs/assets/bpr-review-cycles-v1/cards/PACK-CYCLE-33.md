---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-33"
title: "Publish exact Git objects and a new branch through native REST"
status: "Backlog"
rfcs: ["RFC-BPR-0059"]
depends_on: ["PACK-CYCLE-31", "PACK-CYCLE-32", "PACK-CYCLE-05"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-04", "RC59-05"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-33: Publish exact Git objects and a new branch through native REST

## Outcome and bounded work

Add only the admitted blob/tree/commit/new-ref operations to the GitHub provider through existing native HTTPS and current host/account policy. Verify expected identities and tree/parent/metadata, required permissions and non-overwriting new-ref behavior.

## Acceptance and direct evidence

Isolated provider fixtures for each request/result, permission/protected-path/version/ref-conflict/identity mismatch, quotas/rate limits and unknown host. Keep installed Git credential-free; actual designated live evidence is separately authorized.

Criteria: RC59-04, RC59-05.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
