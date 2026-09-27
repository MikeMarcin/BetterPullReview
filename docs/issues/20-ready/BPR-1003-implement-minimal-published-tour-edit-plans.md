---
schema: "implement-rfc.card.v1"
id: "BPR-1003"
title: "Implement minimal published-tour edit plans"
status: "Ready"
rfcs: ["RFC-BPR-0042"]
depends_on: ["BPR-1002"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-13","milestone":"manual-maintenance","acceptance":["AC42-02","AC42-03","AC42-05"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1003: Implement minimal published-tour edit plans

Pack alias: **PACK-TOUR-13** (live `BPR-1003`, allocated at import).

## Outcome and work

Plan reorder, insertion in a later batch, owned explanation edits, membership removal and linked retarget replacements. Preserve existing IDs/replies. Compute minimal affected ordinal/footer/index edits, retain a valid overview link throughout and update the index after required destinations exist. No-op changes produce no writes; removing a stop is not deleting its thread.

## Existing ownership seams

`Existing mutation/submission contracts and journal; new pure tour update planning`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Creation/edit counts for single-stop move, two-batch insertion, replied-to retirement and immutable-anchor replacement. Kill after new comments, navigation update and index update; reconcile/resume without duplicate comments or destructive rollback.

Requirement ownership: `AC42-02`, `AC42-03`, `AC42-05`.

## Integration boundary

Milestone: **manual-maintenance**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
