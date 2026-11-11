---
schema: "implement-rfc.card.v1"
id: "BPR-2091"
title: "Wait for the accepted published PR snapshot before repaired-tour maintenance preview"
status: "In Progress"
rfcs: ["RFC-BPR-0041", "RFC-BPR-0042"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"milestone_plan":{"id":"bpr-2091-tour-preview-readiness","path":"docs/rfcs/plans/bpr-2091-tour-preview-readiness/milestone-plan.json"},"mandate_revision":"release56-tour-preview-readiness-r1","source_baseline":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","allocation_main":"94928557de405f9401e561fff8e115125eddc9cf","reservation":"docs/issues/_reservations/BPR-2091--BPR-2091--7b08c6e6c706.json","related_cards":["BPR-1981"]}
---

# Wait for the published PR snapshot before repaired-tour maintenance preview

The normal56 Windows Debug repairs course fails its immediate second Preview
while the published local receipt is already saved but the accepted PR snapshot
still contains the pre-publication template. One unchanged-flow diagnostic
reproduces exactly: snapshot1/readbusy1, receipt/saved revision17, server brief
headings present, accepted description missing them, writes13 unchanged.

Recording error9 is the absent optional timeline (operation12) after the strict
base read completed, also observed before the initial publication when Reading
was false. The provider marks that timeline failed and continues the valid PR
read. It is not a recorded-order mismatch or cause for changing the provider.

## Acceptance

- AC1. Preserve both actual failures and causal scalar evidence. Capture the
  accepted snapshot immediately before the original Publish. Before the same
  single second Preview, await a nonempty advanced accepted snapshot and no
  pending/owed/queued read through the existing20s helper. Keep every original
  assertion, gesture, provider operation and90s outer bound.
- AC2. One corrected original native Release and Debug repairs row each passes.
  Retain the original normal and instrumented failures as concrete negatives;
  if root approves the existing withheld-read-terminal control, it must reject
  readiness at the same20s bound and release before every failure/shutdown.
  No diagnostic or old-source result supplies release qualification credit.
- AC3. Focused format22, tidy, unchanged source contract, docs and board gates
  pass. Land only owned test/workflow changes on main, preserve unrelated work
  and counter highwater, return owned commands and native borrows. Root owns
  final common-source qualification and publication.
