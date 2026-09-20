---
schema: "implement-rfc.card.v1"
id: "BPR-0942"
title: "Keep folder gestures separate from semantic file selection"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r21-drawer-and-exact-material"}
---

# Keep folder gestures separate from semantic file selection

Owner-observed signed r22 defects extend the current selected-diff-exact-epoch-pair
milestone. Prior BPR-0935..0939 acceptance remains historical; these are new
follow-ups, not reopened Done cards. Preserve BPR-0934 Overview folder collapse
as separate work and do not request another owner loading test.

## Acceptance

- AC1: Folder rows are structurally non-selectable as files; mouse, keyboard and expand/collapse gestures emit no semantic file selection and do not change the current comparison.
- AC2: Filtering, ordering, overview refresh and model rebuild preserve that boundary and exact surviving file identity; foreign indexes cannot alias files.
- AC3: Failure-before model/gesture checks and production-composed interaction evidence prove no folder-triggered diff, including after rebuild.

## Evidence

Pending direct diagnosis, failing-before regressions and repaired public-path proof.
