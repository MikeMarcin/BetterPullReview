---
schema: "implement-rfc.card.v1"
id: "BPR-1157"
title: "Integrate local entry and source-first tabs with the accepted chrome"
status: "Ready"
rfcs: ["RFC-BPR-0061"]
depends_on: ["BPR-1146", "BPR-1151"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC61-01", "RC61-02", "RC61-03", "RC61-09"], "pack_alias": "PACK-CYCLE-15", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-15** (live `BPR-1157`, allocated at import).

# BPR-1157: Integrate local entry and source-first tabs with the accepted chrome

## Outcome and bounded work

Add Open Project, cycle recents/title badge and R0 full-source view in the RFC0052 scaffold. Preserve title/review bar, fold, per-tab exact selection and tray. Prepare a small native design course for genuinely unresolved layout choices without replacing accepted art.

## Acceptance and direct evidence

No-account entry to first note, native width/height/focus/key checks, tab/source restoration, no extra permanent row, unchanged-file comment action and subject-switch regression to a real PR.

Criteria: RC61-01, RC61-02, RC61-03, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
