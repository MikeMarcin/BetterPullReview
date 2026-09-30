---
schema: "implement-rfc.card.v1"
id: "BPR-1175"
title: "Publish exact Git objects and a new branch through native REST"
status: "Ready"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1173", "BPR-1174", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-04", "RC59-05"], "pack_alias": "PACK-CYCLE-33", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-33** (live `BPR-1175`, allocated at import).

# BPR-1175: Publish exact Git objects and a new branch through native REST

## Outcome and bounded work

Add only the admitted blob/tree/commit/new-ref operations to the GitHub provider through existing native HTTPS and current host/account policy. Verify expected identities and tree/parent/metadata, required permissions and non-overwriting new-ref behavior.

## Acceptance and direct evidence

Isolated provider fixtures for each request/result, permission/protected-path/version/ref-conflict/identity mismatch, quotas/rate limits and unknown host. Keep installed Git credential-free; actual designated live evidence is separately authorized.

Criteria: RC59-04, RC59-05.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
