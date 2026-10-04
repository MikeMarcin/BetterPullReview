---
schema: "implement-rfc.card.v1"
id: "BPR-1171"
title: "Verify desktop/CLI/external-consumer parity"
status: "In Progress"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1166", "BPR-1170"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-03", "RC58-05", "RC58-06", "RC58-07", "RC58-10"], "pack_alias": "PACK-CYCLE-29", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-29** (live `BPR-1171`, allocated at import).

# BPR-1171: Verify desktop/CLI/external-consumer parity

## Outcome and bounded work

Run a common cycle through desktop -> human CLI -> external consumer -> manual capture and back. Verify same frozen payload/IDs, complete reads, source bindings, restored drafts and responses when desktop is open. Include remote/unreachable harness fallback.

## Acceptance and direct evidence

Cross-interface golden payloads, empty/no-JSON human terminal, concurrent focus/draft edits, supported actual harness observation and ordinary file/manual course. Tool reads are not reported as human acceptance.

Criteria: RC58-03, RC58-05, RC58-06, RC58-07, RC58-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
