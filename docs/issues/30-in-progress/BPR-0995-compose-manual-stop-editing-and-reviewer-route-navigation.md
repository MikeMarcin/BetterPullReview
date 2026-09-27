---
schema: "implement-rfc.card.v1"
id: "BPR-0995"
title: "Compose manual stop editing and reviewer route navigation"
status: "In Progress"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040"]
depends_on: ["BPR-0994"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-05","milestone":"manual-authoring","acceptance":["AC39-03","AC39-06","AC40-08"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0995: Compose manual stop editing and reviewer route navigation

Pack alias: **PACK-TOUR-05** (live `BPR-0995`, allocated at import).

## Outcome and work

Add source-selection Add stop, independent title/body/related-source editing, outline insert/reorder/split/merge/remove, keyboard equivalents and preview. Keep selected code and comment visible together. Add Previous/Next stop and a local Return to tour without marking source reviewed or hiding the normal file/change path.

## Existing ownership seams

`Current source selection/diff navigation, conversation model, workbench window and review composition`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Composed manual route with multiple stops in one file and cross-file conceptual order; excursion/return, keyboard reorder, duplicate titles, retained selection and zero viewed/resolve/approve/publication effects.

Requirement ownership: `AC39-03`, `AC39-06`, `AC40-08`.

## Integration boundary

Milestone: **manual-authoring**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
