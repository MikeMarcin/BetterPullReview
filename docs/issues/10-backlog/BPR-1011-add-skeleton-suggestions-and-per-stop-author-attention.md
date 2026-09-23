---
schema: "implement-rfc.card.v1"
id: "BPR-1011"
title: "Add skeleton suggestions and per-stop author attention"
status: "Backlog"
rfcs: ["RFC-BPR-0043"]
depends_on: ["BPR-1010", "BPR-0995"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-21","milestone":"outline-and-adversaries","acceptance":["AC43-08","AC43-09"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1011: Add skeleton suggestions and per-stop author attention

Pack alias: **PACK-TOUR-21** (live `BPR-1011`, allocated at import).

## Outcome and work

Implement optional outline result validation with titles/source refs/private prompts and no publishable bodies. Insert retained entries as one local undoable outline operation. Require visible stop/code visit and current-content Ready-and-next after per-stop manual or explicit AI composition. Preserve free insertion/reordering; no generate-all, auto-advance, bulk Ready or placeholder-visit acceptance.

## Existing ownership seams

`Tour core readiness; outline editor; existing scoped agent delivery`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Extra body/Ready/tool fields rejected; duplicate/nonexistent source refs; no AI on selection; hidden/background visit refusal; output arriving after leaving; regenerated text invalidates readiness; deterministic renumbering does not; every outstanding entry completed or removed before publication.

Requirement ownership: `AC43-08`, `AC43-09`.

## Integration boundary

Milestone: **outline-and-adversaries**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
