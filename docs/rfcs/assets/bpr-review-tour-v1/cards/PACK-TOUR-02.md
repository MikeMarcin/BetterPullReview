---
schema: "implement-rfc.card.v1"
id: "PACK-TOUR-02"
title: "Model tours, stops, source ranges and readiness in the core"
status: "Backlog"
rfcs: ["RFC-BPR-0039"]
depends_on: ["PACK-TOUR-01"]
---

# Model tours, stops, source ranges and readiness in the core

Pack alias: **PACK-TOUR-02**. Allocate/remap a live BPR ID at intake; this is not an already reserved board card.

## Outcome and work

Add stable draft/stop identity, independent problem/solution fields, ordered stop membership, bounded source-range references, original versus mapped destinations, manual completion and explicit incomplete states. Add deterministic edit/reorder/duplicate/split/merge/remove commands with exact generations. Keep review-disposition/read-receipt policy separate.

## Existing ownership seams

`src/core/include/bpr/core/review_round_trip.hpp; review_submission.hpp; revisioned_review_draft.hpp; corresponding core sources`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Core tests for stable IDs, duplicate titles, head/profile replacement, invalid/cross-side ranges, reorder inversion, text-preserving split/merge, empty drafts and safety bounds.

Requirement ownership: `AC39-03`, `AC39-04`, `AC39-08`.

## Integration boundary

Milestone: **manual-authoring**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
