---
schema: "implement-rfc.card.v1"
id: "BPR-0998"
title: "Bind tour code destinations and exact compatibility subjects"
status: "Ready"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041"]
depends_on: ["BPR-0996", "BPR-0997"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-08","milestone":"github-round-trip","acceptance":["AC39-04","AC40-06","AC41-06"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0998: Bind tour code destinations and exact compatibility subjects

Pack alias: **PACK-TOUR-08** (live `BPR-0998`, allocated at import).

## Outcome and work

Validate exact source line/range, commit/blob/path/side identity. Separate native diff, native evidenced off-hunk, file-comment permalink and tour-specific PR-comment fallback for historical/untouched code. Extend range encoding explicitly rather than assuming the old single-line contract. Classify whether each intended destination can join a pending batch.

## Existing ownership seams

`source_line_ref and retained comparison; RFC-BPR-0022 permalink/placement policy; GitHub capability observations`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Current multiline, rename, deleted/base historical, entirely unchanged related file, unavailable source, invalid range, Markdown plain-view link and unsupported/rate-limit/permission responses. No guessed nearest changed line.

Requirement ownership: `AC39-04`, `AC40-06`, `AC41-06`.

## Integration boundary

Milestone: **github-round-trip**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
