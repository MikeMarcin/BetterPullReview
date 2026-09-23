---
schema: "implement-rfc.card.v1"
id: "PACK-TOUR-16"
title: "Prove the complete manual workflow and prepare website acceptance"
status: "Backlog"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042"]
depends_on: ["PACK-TOUR-13", "PACK-TOUR-14", "PACK-TOUR-15"]
---

# Prove the complete manual workflow and prepare website acceptance

Pack alias: **PACK-TOUR-16**. Allocate/remap a live BPR ID at intake; this is not an already reserved board card.

## Outcome and work

Run a full production-port fixture course: draft PR → manually write brief/stops → reorder → preview → staged COMMENT submission → simulated website reply/edit → reopen → insert/reorder → interrupt/reconcile → cold reimport. Prepare the exact designated website/GHES/native course and retain which observations are still unavailable. The manual feature must not depend on AI configuration.

## Existing ownership seams

`Current tests/application and composed app fixture harness; existing designated-live operator workflow`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

One end-to-end fixture result with independent server mutation counters and actual application UI navigation. Prepare, but do not fabricate, website-only/live observations; those remain final release gates in card 23. This milestone can seed later AI engineering once manual behavior is directly proven.

Requirement ownership: `AC39-01`, `AC40-03`, `AC40-07`, `AC41-09`, `AC42-01`, `AC42-08`.

## Integration boundary

Milestone: **manual-maintenance**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
