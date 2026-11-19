---
schema: "implement-rfc.card.v1"
id: "BPR-2284"
title: "The workbench window course still named the tour's removed Problem and Solution Enhance controls"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2284--BPR-2284--3ff49918be79.json","found_by":"coordinator, qt_workbench_agent_send_refusal red on main 143e6f0ef","related_cards":["BPR-2274","BPR-2281"]}
---

# BPR-2284: The workbench course named removed tour controls

## Cause

[[BPR-2274]] (73e9571a1) removed the tour's Problem and Solution and their
Enhance controls, `tourProblemMagic` and `tourSolutionMagic`. The workbench
window course (`tests/adapters_qt/workbench_window_tests.cpp`, rows
`qt_workbench_agent_send_refusal` and its siblings) still required both in
its Agent-accent inventory, and read `tourProblemMagic` for the Tour tab's
drafting before the agent is set up. The row already failed at 795c18861,
before [[BPR-2281]]. The window's hide list in
`workbench_window_github_presentation.cpp` still named both too (harmless).

Separately, [[BPR-2281]]'s Draft Description did not wear a changed Agent
accent on a palette change, which the same inventory caught once the stale
names were gone; that is fixed under BPR-2281's own slice in the same commit.

## Change

- The inventory requires `tourStopMagic` and `tourDraftTour` (Enhance and
  Draft Tour, the tour's run controls since BPR-2274); the pre-setup check
  reads `tourDraftTour`.
- The hide list drops the two removed names.

## Acceptance

- AC1. `qt_workbench_agent_send_refusal` passes.

## Verification

macOS, dev preset, on main 143e6f0ef plus this change:
`qt_workbench_agent_send_refusal`, every `qt_workbench_agent_*` row,
`qt_workbench_window_scale_100` and `architecture_boundaries` pass.
