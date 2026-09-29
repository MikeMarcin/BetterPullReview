---
schema: "implement-rfc.card.v1"
id: "BPR-1144"
title: "Add provider-independent cycle identities and state transitions"
status: "Done"
rfcs: ["RFC-BPR-0053"]
depends_on: ["BPR-1143"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-01", "RC53-02", "RC53-03", "RC53-09"], "pack_alias": "PACK-CYCLE-02", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-01","outcome":"pass","revision":"18655830a62d8b5906cc6e7dd865e007999f4461","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Core share: ctest core_review_cycle passes (dev and core-only presets, macOS arm64). Cycle created with no PR, remote, commit, account or agent; empty complete R0 projects as InitialEmpty while uncaptured stays absent. Reopen through the durable store is BPR-1145 evidence."}},{"criterion":"RC53-02","outcome":"pass","revision":"18655830a62d8b5906cc6e7dd865e007999f4461","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Core share: distinct identity types for cycle, source revision, item, draft, round, response, verification and publication; per-cycle counters only advance, refuse past INT64_MAX, and never reissue; references bind the allocating cycle; no provider id field exists on a cycle subject. Persistence is BPR-1145 evidence."}},{"criterion":"RC53-03","outcome":"pass","revision":"18655830a62d8b5906cc6e7dd865e007999f4461","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Core share: review_subject holds pull_request_authority unchanged as one alternative; no existing PR/epoch type was edited; capture observations are bounded summaries (250,000 entries, titles 256 bytes, 1,024 scope paths). Application ports are BPR-1146 evidence."}},{"criterion":"RC53-09","outcome":"pass","revision":"18655830a62d8b5906cc6e7dd865e007999f4461","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Scope changes are new versions and captures must name the current one; partial-capture, out-of-scope and unavailable paths classify unknown and never compare as deleted; only complete absence or explicit deletion is deletion; narrowing scope keeps history."}},{"criterion":"BPR-1144:boundary","outcome":"pass","revision":"18655830a62d8b5906cc6e7dd865e007999f4461","evidence":{"kind":"command","detail":"architecture_boundaries and architecture_boundary_contract pass with the new core files; tidy.py passes on review_cycle.cpp and its test; clang-format clean; documentation gate DOCUMENTATION_OK."}}]
---

Pack alias: **PACK-CYCLE-02** (live `BPR-1144`, allocated at import).

# BPR-1144: Add provider-independent cycle identities and state transitions

## Outcome and bounded work

Introduce local_cycle as an explicit review subject, with local profile/workspace scope and strong IDs for source revisions, feedback drafts/rounds and verification. Implement finite core transitions and bounded projections. Existing PR authority types remain intact. Define unknown/partial scope and separate ordering/material equality.

## Acceptance and direct evidence

Core tests for no-PR R0, invalid/zero/cross-subject IDs, overflow, unrelated profiles, partial scope and absence versus empty. Assert bounded observations and no provider/Qt/effect access in the core.

Criteria: RC53-01, RC53-02, RC53-03, RC53-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
