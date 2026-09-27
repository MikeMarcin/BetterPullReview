---
schema: "implement-rfc.card.v1"
id: "BPR-1000"
title: "Journal staged publication and unknown outcomes durably"
status: "In Progress"
rfcs: ["RFC-BPR-0041"]
depends_on: ["BPR-0999", "BPR-0993"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-10","milestone":"staged-publication","acceptance":["AC41-02","AC41-03","AC41-05","AC41-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1000: Journal staged publication and unknown outcomes durably

Pack alias: **PACK-TOUR-10** (live `BPR-1000`, allocated at import).

## Outcome and work

Extend the current journal/session with finite plan steps, pre-write baselines, transmission witnesses, owned pending IDs, generated-link association, final membership verification and submitted receipts. Introduce bounded parent-plan authorization and exact one-use child grants. Reconcile ambiguous writes read-only; never infer failure from an empty or partial observation. Resume after restart requires explicit authorization.

## Existing ownership seams

`src/adapters/github/review_submission_session.cpp; existing review submission recovery port and SQLite store`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Kill before/after each witness, lost creation/edit/submit response, contaminated pending review, identical concurrent candidate ambiguity, external early submission and changed head. Independent fake server counters prove no duplicate create or submit.

Requirement ownership: `AC41-02`, `AC41-03`, `AC41-05`, `AC41-08`.

## Integration boundary

Milestone: **staged-publication**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
