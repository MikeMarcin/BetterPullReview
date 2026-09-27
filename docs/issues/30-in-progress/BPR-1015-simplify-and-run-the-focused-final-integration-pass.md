---
schema: "implement-rfc.card.v1"
id: "BPR-1015"
title: "Simplify and run the focused final integration pass"
status: "In Progress"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: ["BPR-1012", "BPR-1014"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-25","milestone":"qualification-closeout","acceptance":[],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1015: Simplify and run the focused final integration pass

Pack alias: **PACK-TOUR-25** (live `BPR-1015`, allocated at import).

## Outcome and work

Remove duplicate publishers, redundant state/undo paths, obsolete temporary adapters and unnecessary schema/platform abstraction introduced by the feature. Preserve behavior. Run scoped formatting/tidy, documentation and one relevant final integrated test batch using current project presets and isolation. Reuse unchanged qualified evidence; record unrelated baseline failures separately.

## Existing ownership seams

`Changed production seams, associated CMake/CTest/packaging resources and documentation`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Behavior-preserving review plus final relevant core/application/provider/SQLite/Qt integration results. No weakened limits, replaced assertions, fabricated success markers or blanket re-running every project matrix to hide missing focused evidence.

## Integration boundary

Milestone: **qualification-closeout**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
