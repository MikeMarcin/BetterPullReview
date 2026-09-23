---
schema: "implement-rfc.card.v1"
id: "PACK-TOUR-01"
title: "Import the authored pack and reconcile the live baseline"
status: "Backlog"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: []
---

# Import the authored pack and reconcile the live baseline

Pack alias: **PACK-TOUR-01**. Allocate/remap a live BPR ID at intake; this is not an already reserved board card.

## Outcome and work

Record live HEAD, current RFC/issue allocations, current test isolation and affected in-flight ownership. Reserve/remap proposed RFC IDs if occupied, allocate issue IDs with the existing allocator, retain receipts, import the authored milestone plan with the installed skill, and preserve this archive as source. Check the top-level source against SOURCE-BASELINE rather than embedded worktree copies. Record exact existing publisher, Markdown, editor, settings and storage seams once.

## Existing ownership seams

`AGENTS.md; docs/rfcs/README.md; scripts/allocate_issue_id.py; .mask/skill-config/implement-rfc/`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Native plan/board validation after alias remap; explicit mapping for all five RFCs and 26 card aliases; one source-drift note. Do not run company PR mutations or change existing RFC status.

## Integration boundary

Milestone: **intake**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
