---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-27"
title: "Expose scoped prepared-round retrieval and response-only grants"
status: "Backlog"
rfcs: ["RFC-BPR-0058", "RFC-BPR-0060"]
depends_on: ["PACK-CYCLE-05", "PACK-CYCLE-25", "PACK-CYCLE-26"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-03", "RC58-04", "RC58-07", "RC58-09"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-27: Expose scoped prepared-round retrieval and response-only grants

## Outcome and bounded work

Implement exact cycle/round selection, capabilities, complete read/page/source and optional response append. CWD resolves only unambiguous allowed metadata. Scope reads to ready payload/context; imported IDs are not grants. Add consistent policy decisions to GUI/CLI paths.

## Acceptance and direct evidence

Two active cycles, focus change, revoked scope, hidden draft/context, every-page completion, oversize partition refusal, response grant attempting verify/publish and unknown provider identity. No direct database or generic filesystem endpoint.

Criteria: RC58-03, RC58-04, RC58-07, RC58-09.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
