---
schema: "implement-rfc.card.v1"
id: "BPR-1158"
title: "Freeze deterministic complete feedback payloads"
status: "Ready"
rfcs: ["RFC-BPR-0056"]
depends_on: ["BPR-1153", "BPR-1154", "BPR-1155"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-01", "RC56-02", "RC56-06", "RC56-09"], "pack_alias": "PACK-CYCLE-16", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-16** (live `BPR-1158`, allocated at import).

# BPR-1158: Freeze deterministic complete feedback payloads

## Outcome and bounded work

Implement one canonical round assembler using versioned selections, exact suggestions, deduplicated explicit source context and assignment/order. Freeze after safe editor flush; keep immutable digest and successor amendments. No required AI summarization or second prompt.

## Acceptance and direct evidence

Golden canonical JSON/digests, identical re-freeze reuse, reordered items change digest, CRLF/base64 exactness, hidden-turn exclusion, IME/busy/stale draft and interrupted freeze recovery through production services.

Criteria: RC56-01, RC56-02, RC56-06, RC56-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
