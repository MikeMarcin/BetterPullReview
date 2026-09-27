---
schema: "implement-rfc.card.v1"
id: "BPR-0999"
title: "Extend native GitHub ports for staged review batches"
status: "Ready"
rfcs: ["RFC-BPR-0041"]
depends_on: ["BPR-0997", "BPR-0998"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-09","milestone":"staged-publication","acceptance":["AC41-01","AC41-02","AC41-06","AC41-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0999: Extend native GitHub ports for staged review batches

Pack alias: **PACK-TOUR-09** (live `BPR-0999`, allocated at import).

## Outcome and work

Add typed pending creation, exact review/comment enumeration, owned body updates, exact submission and optional qualified file-thread staging. Compile documented payloads with omitted creation event and COMMENT submission. Map local stops from complete observed identities rather than response ordering. Keep summary-only and source-line interfaces/regressions compatible.

## Existing ownership seams

`src/application review_submission contracts; src/adapters/github/github_mutation_contract.cpp; finite GraphQL operation policy`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Production contract tests for exact methods/paths/payloads, unsupported standalone fields rejected in batch, GraphQL review IDs/subject type, full IDs, complete membership, response budgets and API-version profile selection.

Requirement ownership: `AC41-01`, `AC41-02`, `AC41-06`, `AC41-08`.

## Integration boundary

Milestone: **staged-publication**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
