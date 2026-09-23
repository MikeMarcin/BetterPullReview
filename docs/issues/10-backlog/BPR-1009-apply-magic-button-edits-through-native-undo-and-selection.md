---
schema: "implement-rfc.card.v1"
id: "BPR-1009"
title: "Apply magic-button edits through native undo and selection"
status: "Backlog"
rfcs: ["RFC-BPR-0043"]
depends_on: ["BPR-1008", "BPR-0994"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-19","milestone":"scoped-ai","acceptance":["AC43-03","AC43-04","AC43-05","AC43-06","AC43-10"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1009: Apply magic-button edits through native undo and selection

Pack alias: **PACK-TOUR-19** (live `BPR-1009`, allocated at import).

## Outcome and work

Add secondary magic buttons with scope-sensitive labels and preserved selection before focus transfer. Keep text unchanged while running, apply completed output as one distinct undo transaction, restore exact selection/text on undo and replay received bytes on redo. Preserve raw UTF-8/CRLF versus Qt UTF-16 selection mappings. Do not mount Original/Proposed or Apply/Dismiss widgets and do not setPlainText on each model acknowledgement.

## Existing ownership seams

`Qt native Markdown prose editors; existing editor undo machinery; incremental projection`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Actual Qt edit history tests with adjacent typing, repeated Enhance, selected replacement, toolbar focus, moved caret, emoji/surrogates, combining text, tabs, CRLF/LF, IME, multiline selection, cancellation, zero/equal/invalid output and redo with zero agent calls.

Requirement ownership: `AC43-03`, `AC43-04`, `AC43-05`, `AC43-06`, `AC43-10`.

## Integration boundary

Milestone: **scoped-ai**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
