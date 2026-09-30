---
schema: "implement-rfc.card.v1"
id: "BPR-1173"
title: "Build isolated squashed candidates with exact transformation limits"
status: "Ready"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1172", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-02", "RC59-04"], "pack_alias": "PACK-CYCLE-31", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-31** (live `BPR-1173`, allocated at import).

# BPR-1173: Build isolated squashed candidates with exact transformation limits

## Outcome and bounded work

Construct a one-parent outgoing tree/commit in a private object store/index using controlled local plumbing. Bind author/committer metadata and transformations. Detect unsupported LFS/filter/signing/object-format cases; keep source checkout untouched.

## Acceptance and direct evidence

Hash user index/refs/files before/after, malicious config/hook/filter canaries, LF transform preview, binary/mode/symlink cases, LFS pointer versus bytes, SHA-1/SHA-256 admission and external-preparation output.

Criteria: RC59-02, RC59-04.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
