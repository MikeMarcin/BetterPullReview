---
schema: "implement-rfc.card.v1"
id: "BPR-0993"
title: "Persist revisioned tour drafts and scoped remote baselines"
status: "Ready"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0042"]
depends_on: ["BPR-0992"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-03","milestone":"manual-authoring","acceptance":["AC39-05","AC42-07"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0993: Persist revisioned tour drafts and scoped remote baselines

Pack alias: **PACK-TOUR-03** (live `BPR-0993`, allocated at import).

## Outcome and work

Add admitted bounded tour draft records, stop order, preparation metadata and raw-body baselines using existing private-store/revision patterns. Document the authored-text/remote-baseline retention extension. Implement debounced or coalesced autosave without losing final edits, restart loading, account/profile partitioning and optimistic multiwindow conflicts. Preserve existing schemas and recovery rows.

## Existing ownership seams

`src/adapters/sqlite profile/private-store paths; src/core/include/bpr/core/revisioned_review_draft.hpp; application serialized host lane`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Real SQLite migration/rollback/reopen, killed-save recovery, wrong-profile access, lost-save status and two-window conflicting generations. Confirm no remote operation occurs from save or restart.

Requirement ownership: `AC39-05`, `AC42-07`.

## Integration boundary

Milestone: **manual-authoring**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
