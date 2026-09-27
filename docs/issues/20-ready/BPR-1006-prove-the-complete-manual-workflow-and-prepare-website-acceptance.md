---
schema: "implement-rfc.card.v1"
id: "BPR-1006"
title: "Prove the complete manual workflow and prepare website acceptance"
status: "Ready"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042"]
depends_on: ["BPR-1003", "BPR-1004", "BPR-1005"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-16","milestone":"manual-maintenance","acceptance":["AC39-01","AC40-03","AC40-07","AC41-09","AC42-01","AC42-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1006: Prove the complete manual workflow and prepare website acceptance

Pack alias: **PACK-TOUR-16** (live `BPR-1006`, allocated at import).

## Outcome and work

Run a full production-port fixture course: draft PR → manually write brief/stops → reorder → preview → staged COMMENT submission → simulated website reply/edit → reopen → insert/reorder → interrupt/reconcile → cold reimport. Prepare the exact designated website/GHES/native course and retain which observations are still unavailable. The manual feature must not depend on AI configuration.

## Existing ownership seams

`Current tests/application and composed app fixture harness; existing designated-live operator workflow`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

One end-to-end fixture result with independent server mutation counters and actual application UI navigation. Prepare, but do not fabricate, website-only/live observations; those remain final release gates in card 23. This milestone can seed later AI engineering once manual behavior is directly proven.

Requirement ownership: `AC39-01`, `AC40-03`, `AC40-07`, `AC41-09`, `AC42-01`, `AC42-08`.

## Integration boundary

Milestone: **manual-maintenance**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
