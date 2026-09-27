---
schema: "implement-rfc.card.v1"
id: "BPR-0999"
title: "Extend native GitHub ports for staged review batches"
status: "Done"
rfcs: ["RFC-BPR-0041"]
depends_on: ["BPR-0997", "BPR-0998"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-09","milestone":"staged-publication","acceptance":["AC41-01","AC41-02","AC41-06","AC41-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC41-01","outcome":"pass","revision":"f00c47610db4a3235dd2529f0d404e095c8b4593","evidence":{"kind":"observation","detail":"Finite pending creation omits event; exact returned review ID COMMENT submission compiles independently; native physical verb fixture and existing summary/source-line regressions pass. Coordinator is BPR-1000/1002.","ledger":"docs/verification/RFC-BPR-0039-0043.md#typed-pending-ports--bpr-0999"}},{"criterion":"AC41-02","outcome":"pass","revision":"f00c47610db4a3235dd2529f0d404e095c8b4593","evidence":{"kind":"observation","detail":"Raw decoder preserves returned exact IDs/URLs/actors/membership/source ranges and pagination obligations; duplicate/foreign identities fail. Complete multiset reconciliation is BPR-1000.","ledger":"docs/verification/RFC-BPR-0039-0043.md#typed-pending-ports--bpr-0999"}},{"criterion":"AC41-06","outcome":"pass","revision":"f00c47610db4a3235dd2529f0d404e095c8b4593","evidence":{"kind":"observation","detail":"Only documented line/range batch fields compile. No pending file-thread operation is qualified or enabled; live qualification remains later assigned scope.","ledger":"docs/verification/RFC-BPR-0039-0043.md#typed-pending-ports--bpr-0999"}},{"criterion":"AC41-08","outcome":"pass","revision":"f00c47610db4a3235dd2529f0d404e095c8b4593","evidence":{"kind":"observation","detail":"5/5 focused contract/native/regression/boundary checks pass; exact paths and payloads, budgets, pagination, redirects and non-success responses covered. No live endpoint qualification claimed.","ledger":"docs/verification/RFC-BPR-0039-0043.md#typed-pending-ports--bpr-0999"}}]
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
