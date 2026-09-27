---
schema: "implement-rfc.card.v1"
id: "BPR-0994"
title: "Build the Problem and Solution authoring surface"
status: "In Progress"
rfcs: ["RFC-BPR-0039"]
depends_on: ["BPR-0992", "BPR-0993"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-04","milestone":"manual-authoring","acceptance":["AC39-01","AC39-02","AC39-06","AC39-07"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0994: Build the Problem and Solution authoring surface

Pack alias: **PACK-TOUR-04** (live `BPR-0994`, allocated at import).

## Outcome and work

Mount Prepare review tour in the current workbench with revisitable Problem and Solution Markdown fields, manual mapping entry points, native undo/redo, short coaching and optional existing safe preview. Keep original raw source as publication authority. No AI setup, launch or mutation occurs on entry, typing, section change or rendering.

## Existing ownership seams

`src/adapters/qt review editors and markdown_body_view; current src/app composition under RFC-BPR-0037`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Qt/application public path with no agent configured; keyboard/IME/clipboard, raw Markdown source/preview, source-unavailable/empty/error states, section revisit and local save failure.

Requirement ownership: `AC39-01`, `AC39-02`, `AC39-06`, `AC39-07`.

## Integration boundary

Milestone: **manual-authoring**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
