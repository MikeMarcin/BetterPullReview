---
schema: "implement-rfc.card.v1"
id: "BPR-1010"
title: "Build bounded context and explicit per-field agent delivery"
status: "Backlog"
rfcs: ["RFC-BPR-0043"]
depends_on: ["BPR-1007", "BPR-1008", "BPR-1009"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-20","milestone":"scoped-ai","acceptance":["AC43-01","AC43-07","AC43-09","AC43-10"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1010: Build bounded context and explicit per-field agent delivery

Pack alias: **PACK-TOUR-20** (live `BPR-1010`, allocated at import).

## Outcome and work

Reuse explicit agent invocation with bounded current field/selection, surrounding prose, approved problem/solution/source and author comments. For empty input recover existing statements before inference. Validate strict tagged results, keep recovered/inferred/uncertain notes private and fail safely on unsupported facts/insufficient context. Do not fetch Jira/issues/arbitrary links or expose mutation credentials/tools.

## Existing ownership seams

`Existing agent_invocation_host/session/context supplements and workshop delivery; approved raw PR/source observations`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Canned-agent production-path tests for source provenance, stale context, prompt injection, extra fields, missing evidence, invalid UTF-8, over-budget output, late responses and credential/content log canaries. Qualitative prompt examples do not certify model correctness.

Requirement ownership: `AC43-01`, `AC43-07`, `AC43-09`, `AC43-10`.

## Integration boundary

Milestone: **scoped-ai**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
