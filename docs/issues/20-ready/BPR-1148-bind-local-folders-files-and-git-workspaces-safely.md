---
schema: "implement-rfc.card.v1"
id: "BPR-1148"
title: "Bind local folders, files and Git workspaces safely"
status: "Ready"
rfcs: ["RFC-BPR-0054"]
depends_on: ["BPR-1146", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-01", "RC54-03", "RC54-10"], "pack_alias": "PACK-CYCLE-06", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-06** (live `BPR-1148`, allocated at import).

# BPR-1148: Bind local folders, files and Git workspaces safely

## Outcome and bounded work

Implement deliberate root selection and logical identity for plain folders/files, Git roots and linked worktrees. Resolve descendants with native no-follow/identity checks. Keep root rebind explicit and map imported logical projects without trusting archive paths.

## Acceptance and direct evidence

Disposable roots for symlink/junction escape, replaced root, linked worktree, two same-named checkouts, moved root, nonrepository file and case/normalization collisions. Record each native platform result separately.

Criteria: RC54-01, RC54-03, RC54-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
