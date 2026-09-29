---
schema: "implement-rfc.card.v1"
id: "BPR-1147"
title: "Implement local scope and disclosure policy primitives"
status: "Done"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060"]
depends_on: ["BPR-1144"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-10", "RC60-04", "RC60-05"], "pack_alias": "PACK-CYCLE-05", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-10","outcome":"pass","revision":"ac139149206893998a5ed4bd6a2a1f1cce658014","evidence":{"kind":"artifact","path":"docs/architecture/security-threat-model.md","detail":"Card share: the threat model's Review Cycles section now records the implemented core grant decision, response-only grant, channel defaults and denial consistency, no-downgrade classification and the absence of any execute capability, beside the intake scope registration. BPR-1185 owns the final documentation share."}},{"criterion":"RC60-04","outcome":"pass","revision":"ac139149206893998a5ed4bd6a2a1f1cce658014","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Core share: core_review_cycle_policy passes a 31-row table; one authorize() decision serves every interface; a denied channel gives channel_denied for reviewer export and consumer read alike; unclassified and enterprise external channels are unconfigured by default; grants are host-stored and a consumer cannot choose an export channel. Interface wiring is owned by BPR-1181 (PACK-CYCLE-39)."}},{"criterion":"RC60-05","outcome":"pass","revision":"ac139149206893998a5ed4bd6a2a1f1cce658014","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Core share: imported content is refused for every capability even with a grant attached; imported 'verified'/'approved' and command-bearing text parse as no claim and a claim never sets the reviewer decision; there is no execute capability. Markdown, bundle and adapter surfaces are owned by BPR-1181 and BPR-1183."}},{"criterion":"BPR-1147:boundary","outcome":"pass","revision":"ac139149206893998a5ed4bd6a2a1f1cce658014","evidence":{"kind":"command","detail":"architecture_boundaries and architecture_boundary_contract pass; tidy.py passes on the policy source and test; clang-format clean; documentation gate DOCUMENTATION_OK."}}]
---

Pack alias: **PACK-CYCLE-05** (live `BPR-1147`, allocated at import).

# BPR-1147: Implement local scope and disclosure policy primitives

## Outcome and bounded work

Add local workspace/round capability types, explicit origin/classification and channel policy decisions. Separate capture, freeze, export, implement instruction, response append, reviewer verification and publication authorization. Define bounded errors rather than implicit global access.

## Acceptance and direct evidence

Core policy table covering unknown folder origin, known enterprise data, denied channel, response-only grants, imported claimed approval and source snippets containing commands.

Criteria: RC53-10, RC60-04, RC60-05.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
