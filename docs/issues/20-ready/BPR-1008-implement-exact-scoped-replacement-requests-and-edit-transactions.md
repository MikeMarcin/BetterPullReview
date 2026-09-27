---
schema: "implement-rfc.card.v1"
id: "BPR-1008"
title: "Implement exact scoped replacement requests and edit transactions"
status: "Ready"
rfcs: ["RFC-BPR-0043"]
depends_on: ["BPR-1007", "BPR-0992"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-18","milestone":"scoped-ai","acceptance":["AC43-03","AC43-04","AC43-05","AC43-06"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1008: Implement exact scoped replacement requests and edit transactions

Pack alias: **PACK-TOUR-18** (live `BPR-1008`, allocated at import).

## Outcome and work

Capture selection/whole-field/empty-field scope and immutable editor/account/PR/source/hook generation. Validate model output as replacement text only and assemble prefix/replacement/suffix in the application/core path. Add cancellation on text revision, scope/editor replacement and supersession. Treat identical result as no-op; monotonic generations prevent undo-back from reviving old output.

## Existing ownership seams

`Core bounded draft commands and revision identities; application request/completion lane; prose editor binding`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Core/property tests for UTF-8 boundaries, whitespace-selection precedence, no outside writes, invalid offsets, duplicate completions, equal-result no-op, undo-back-to-identical text, newer response winning and wrong-target result rejection.

Requirement ownership: `AC43-03`, `AC43-04`, `AC43-05`, `AC43-06`.

## Integration boundary

Milestone: **scoped-ai**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
