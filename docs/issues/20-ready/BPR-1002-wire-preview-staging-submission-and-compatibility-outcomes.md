---
schema: "implement-rfc.card.v1"
id: "BPR-1002"
title: "Wire preview, staging, submission and compatibility outcomes"
status: "Ready"
rfcs: ["RFC-BPR-0041"]
depends_on: ["BPR-1000", "BPR-1001", "BPR-0995"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-12","milestone":"staged-publication","acceptance":["AC41-01","AC41-03","AC41-06","AC41-07","AC41-09"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1002: Wire preview, staging, submission and compatibility outcomes

Pack alias: **PACK-TOUR-12** (live `BPR-1002`, allocated at import).

## Outcome and work

Connect the real preparation surface to immutable previewed plans and exact actor/head identity. Stage and wire actual URLs, submit only the verified owned review, and show distinct local/staged/unknown/published/partial states. Separate optional PR-description/backlink updates. Require explicit non-atomic compatibility choice before visible standalone fallback writes; preserve every completed receipt.

## Existing ownership seams

`src/app/src/review_submission_workflow.cpp; current Qt editor/control projection; application host composition`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Normal app fixture path from empty draft to linked submitted review; pending conflict; description failure after success; stop/Resume; unsupported batching; zero writes before Publish; no extra per-navigation-patch modal and no broad write grants.

Requirement ownership: `AC41-01`, `AC41-03`, `AC41-06`, `AC41-07`, `AC41-09`.

## Integration boundary

Milestone: **staged-publication**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
