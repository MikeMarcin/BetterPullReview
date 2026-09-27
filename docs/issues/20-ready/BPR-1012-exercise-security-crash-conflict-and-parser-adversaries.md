---
schema: "implement-rfc.card.v1"
id: "BPR-1012"
title: "Exercise security, crash, conflict and parser adversaries"
status: "Ready"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: ["BPR-1011", "BPR-1000", "BPR-1004"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-22","milestone":"outline-and-adversaries","acceptance":["AC40-05","AC41-03","AC41-05","AC41-08","AC42-04","AC42-05","AC43-06","AC43-09"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1012: Exercise security, crash, conflict and parser adversaries

Pack alias: **PACK-TOUR-22** (live `BPR-1012`, allocated at import).

## Outcome and work

Run the verification matrix adversaries through production paths: malicious Markdown/URLs, parser bounds, cross-profile references, unknown writes, membership contamination, account switching, simultaneous edits and prompt injection. Ensure finite capability/plan authority, safe cancellation and no secret logging. Keep integration tests behavioral rather than source-string-only.

## Existing ownership seams

`Existing network-isolated CTest, fake GitHub service, real private SQLite, process-kill and agent fixture facilities`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Failure at each mutation phase; full versus partial pagination; process death; HTML/resource and URL credential canaries; shape/ID/bounds fuzz cases; no global auth mutation, no extra inference/publication, no loss of preexisting recovery rows.

Requirement ownership: `AC40-05`, `AC41-03`, `AC41-05`, `AC41-08`, `AC42-04`, `AC42-05`, `AC43-06`, `AC43-09`.

## Integration boundary

Milestone: **outline-and-adversaries**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
