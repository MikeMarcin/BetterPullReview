---
schema: "implement-rfc.card.v1"
id: "BPR-0921"
title: "Investigate No updates loaded after opening a PR"
status: "Backlog"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0029", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Investigate No updates loaded after opening a PR

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-04. Earlier RFC references retain their governing contracts.

## Owner feedback

“Updates says ‘No updates loaded’ at the top where epochs should be.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Trace the first loaded PR through history acquisition, persistence, update
identity and epoch-line projection. Determine whether history was never
requested, was refused/failed, is pending, or exists but is not projected.
Do not infer an empty remote history from the displayed text, and do not invent
updates to fill the strip. Investigate correlation with [[BPR-0922]] without
assuming the two symptoms share a cause.

Starting points: `src/adapters/qt/src/epoch_line.cpp`,
`src/adapters/qt/src/epoch_history_model.cpp`, and the composed history host.

## Acceptance

- Reproduce initial PR open with recorded GHES/CLI fixtures and trace the first missing or refused update-history transition.
- For available history, display the actual update sequence after PR load; loading, genuine empty, unavailable and failed states are distinguishable.
- Refresh and PR/account replacement cannot retain another PR’s epochs or install stale history; record whether the source-loading failure shares the cause.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
