---
schema: "implement-rfc.card.v1"
id: "BPR-1181"
title: "Qualify security boundaries and blocked capability behavior"
status: "Ready"
rfcs: ["RFC-BPR-0060"]
depends_on: ["BPR-1179", "BPR-1180", "BPR-1162"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-04", "RC60-05", "RC60-06", "RC60-10"], "pack_alias": "PACK-CYCLE-39", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-39** (live `BPR-1181`, allocated at import).

# BPR-1181: Qualify security boundaries and blocked capability behavior

## Outcome and bounded work

Audit all channel/capability paths for consistent export policy, known-origin preservation, untrusted reports/code, IPC bounds/identity and log redaction. Document same-user/clipboard/external-harness residual risks and supported unsupported cases.

## Acceptance and direct evidence

Attack corpus, revoked grant/wrong owner, hidden history request, forged model/approval/verification, hostile Markdown/paths and content-free diagnostics. Critical review records code/version/environment, not just policy prose.

Criteria: RC60-04, RC60-05, RC60-06, RC60-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
