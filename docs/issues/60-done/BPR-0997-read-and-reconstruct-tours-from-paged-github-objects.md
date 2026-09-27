---
schema: "implement-rfc.card.v1"
id: "BPR-0997"
title: "Read and reconstruct tours from paged GitHub objects"
status: "Done"
rfcs: ["RFC-BPR-0040"]
depends_on: ["BPR-0996"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-07","milestone":"github-round-trip","acceptance":["AC40-02","AC40-03","AC40-05"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC40-02","outcome":"pass","revision":"f44c95dd","evidence":{"kind":"command","detail":"Paged 101-comment raw course, core adversarial import and actual cold-profile editor/SQLite reconstruction passed; preserves identities/index order/raw spans and refuses foreign/partial/ambiguous write ownership. See milestone ledger exact commands and diagnostic corrections. No live/native qualification claimed."}},{"criterion":"AC40-03","outcome":"pass","revision":"f44c95dd","evidence":{"kind":"command","detail":"Paged 101-comment raw course, core adversarial import and actual cold-profile editor/SQLite reconstruction passed; preserves identities/index order/raw spans and refuses foreign/partial/ambiguous write ownership. See milestone ledger exact commands and diagnostic corrections. No live/native qualification claimed."}},{"criterion":"AC40-05","outcome":"pass","revision":"f44c95dd","evidence":{"kind":"command","detail":"Paged 101-comment raw course, core adversarial import and actual cold-profile editor/SQLite reconstruction passed; preserves identities/index order/raw spans and refuses foreign/partial/ambiguous write ownership. See milestone ledger exact commands and diagnostic corrections. No live/native qualification claimed."}}]
---

# BPR-0997: Read and reconstruct tours from paged GitHub objects

Pack alias: **PACK-TOUR-07** (live `BPR-0997`, allocated at import).

## Outcome and work

Add bounded raw description/review/comment observations and candidate discovery from normal index links. Resolve exact same-PR entities/actors and remote kinds, paginate or expose incomplete state, allow manual index/list choice, and restore published editors after local database loss. Treat third-party objects as read-only unless exact supported edit authority exists.

## Existing ownership seams

`src/adapters/github github_read_contract/github_paged_read/provider paths; application read host`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Cold profile, no AI, paged/reordered collections, missing/duplicate/cross-PR comments, huge uint64 IDs, multiple roots, deleted root, partial/failed reads and invalid URL/credential forwarding negatives.

Requirement ownership: `AC40-02`, `AC40-03`, `AC40-05`.

## Integration boundary

Milestone: **github-round-trip**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
