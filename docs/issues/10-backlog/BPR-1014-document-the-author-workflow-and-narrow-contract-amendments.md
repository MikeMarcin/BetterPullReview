---
schema: "implement-rfc.card.v1"
id: "BPR-1014"
title: "Document the author workflow and narrow contract amendments"
status: "Backlog"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: ["BPR-1006", "BPR-1010", "BPR-1011"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-24","milestone":"qualification-closeout","acceptance":[],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1014: Document the author workflow and narrow contract amendments

Pack alias: **PACK-TOUR-24** (live `BPR-1014`, allocated at import).

## Outcome and work

Apply INTEGRATION-MAP amendments for PR body-only edits, grouped finite tour authority, local in-place AI replacement, range/fallback scope and bounded private baselines. Document no-AI preparation, helpful tour writing, settings hooks, undo, cold reimport, website conflicts, partial updates and unsupported hosts. Preserve old RFCs and unrelated workshop behavior; include sources and limitations honestly.

## Existing ownership seams

`Product/architecture/security/storage/UX/operator documentation and current feature entry points`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Documentation/board validators and an operator walkthrough using normal controls. Confirm no instruction requires hidden markers, AI, BPR for reviewers, token extraction or automatic repository/ticket execution.

## Integration boundary

Milestone: **qualification-closeout**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
