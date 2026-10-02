---
schema: "implement-rfc.card.v1"
id: "BPR-1169"
title: "Expose scoped prepared-round retrieval and response-only grants"
status: "In Progress"
rfcs: ["RFC-BPR-0058", "RFC-BPR-0060"]
depends_on: ["BPR-1147", "BPR-1167", "BPR-1168"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-03", "RC58-04", "RC58-07", "RC58-09"], "pack_alias": "PACK-CYCLE-27", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-27** (live `BPR-1169`, allocated at import).

# BPR-1169: Expose scoped prepared-round retrieval and response-only grants

## Outcome and bounded work

Implement exact cycle/round selection, capabilities, complete read/page/source and optional response append. CWD resolves only unambiguous allowed metadata. Scope reads to ready payload/context; imported IDs are not grants. Add consistent policy decisions to GUI/CLI paths.

## Acceptance and direct evidence

Two active cycles, focus change, revoked scope, hidden draft/context, every-page completion, oversize partition refusal, response grant attempting verify/publish and unknown provider identity. No direct database or generic filesystem endpoint.

Criteria: RC58-03, RC58-04, RC58-07, RC58-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
