---
schema: "implement-rfc.card.v1"
id: "BPR-1007"
title: "Add four versioned writing hooks to profile settings"
status: "Ready"
rfcs: ["RFC-BPR-0043"]
depends_on: ["BPR-1006"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-17","milestone":"scoped-ai","acceptance":["AC43-01","AC43-02"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1007: Add four versioned writing hooks to profile settings

Pack alias: **PACK-TOUR-17** (live `BPR-1007`, allocated at import).

## Outcome and work

Ship Problem, Solution, Outline and Tour-comment text defaults with a fixed application contract. Add profile-local overrides, restore-default, effective version/source and explicit nonpublishing test. Use existing model/provider selection. Loading or changing settings must not launch inference; repository-supplied instructions cannot silently replace user-approved hooks.

## Existing ownership seams

`Current profile settings store/authority; src/adapters/qt/src/settings_window.cpp; existing agent profiles`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Default/override/reset persistence, sibling profile isolation, malformed/oversize config, settings reload during pending request, no-AI setup path and explicit-only hook test.

Requirement ownership: `AC43-01`, `AC43-02`.

## Integration boundary

Milestone: **scoped-ai**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
