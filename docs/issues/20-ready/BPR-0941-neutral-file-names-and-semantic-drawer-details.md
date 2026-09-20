---
schema: "implement-rfc.card.v1"
id: "BPR-0941"
title: "Keep file names neutral and color only change details"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r21-drawer-and-exact-material"}
---

# Keep file names neutral and color only change details

Owner-observed signed r22 defects extend the current selected-diff-exact-epoch-pair
milestone. Prior BPR-0935..0939 acceptance remains historical; these are new
follow-ups, not reopened Done cards. Preserve BPR-0934 Overview folder collapse
as separate work and do not request another owner loading test.

## Acceptance

- AC1: File names use ordinary native text; only added/removed markers and separate plus/minus counts use their semantic colors, matching Overview.
- AC2: Native selection/focus, accessible operation/count text and light/dark/high-contrast readability remain intact.
- AC3: Failure-before pixel checks and the actual composed drawer prove neutral names and independently colored details; rendered safe evidence is inspected.

## Evidence

Pending direct diagnosis, failing-before regressions and repaired public-path proof.
