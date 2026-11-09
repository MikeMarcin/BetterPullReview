---
schema: "implement-rfc.card.v1"
id: "BPR-1971"
title: "Wait for both agent history loads before enabling Send"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"kind":"bug","found_by":"aa7 Windows original native-session wrapper engineering","mandate":"Coordinator amendment r4 approves the confirmed seven-path readiness correction, unchanged guards/oracles and current-source Windows/Mac distinguishing proofs on2026-11-09.","milestone_plan":{"id":"bpr-1971-native-launch-admission","path":"docs/rfcs/plans/bpr-1971-native-launch-admission/milestone-plan.json"}}
---

# BPR-1971: Wait for both agent history loads before enabling Send

- AC1. Bind the original failure and controlled Windows/Mac counterexample:
  selected native history is settled while the actual legacy conversation
  is not loaded, yet Send is enabled and the unique loading guard refuses it.
  Retain original failures, all observations and unreached courses.
- AC2. Project combined readiness from both callbacks with the existing
  refresh path and a cheap host Loaded accessor. Preserve Stop, the 1965
  storage-status return and existing launch guard. A late controlled hold
  rejects the old UI; corrected code waits for genuine readiness, then runs
  the original single unsupported request with its unchanged private-storage,
  no-process-start and no-fallback oracles and 400/180-second limits.
- AC3. Reconcile the current 1972 large-project owner amendment, review and
  reseal only the unchanged-inventory 142-file content binding, complete
  focused Windows/Mac positive/negative proofs plus required Windows host
  and real entry-limit course at original 180 seconds, format/tidy/source/docs
  gates and normal main integration. Commands finish and temporary variants
  are restored. Final release qualification remains separate.

## Result

Both history callbacks now publish the combined loading state and use the
existing refresh path. Native history finishing first keeps Send disabled
until the legacy load settles; either completion order reaches the same
ready state. A scalar host accessor avoids copying saved turns. The existing
launch guard, Stop behavior, storage-status early return and native body
projection remain intact. Current owner policy allowing large workspaces
and the additional successful-turn/history expectations are preserved.

Source candidate `970812bfe2a1107625c788a2e87d2a220e4ba93d` is merged and
pushed through main `2d0c3265d2262f713c60c58af70e29f01c6eeb57`, with all
seven tested source hashes verified. The corrected original six-course
wrapper passes on Windows and Mac. The same regression with old production
presentation code fails the exact readiness assertion on both platforms.
Windows host default and the real 263169-entry course pass; the latter
finishes in 140.38 seconds under its original 180-second watchdog.

Format22, scoped LLVM22 tidy, architecture boundaries, reviewed source
content and documentation gates pass. Independent peer and coordinator
source review have no blocking findings. Original failures, inconclusive
traces and controlled counterexamples retain their actual results. All
variants are retired by normal restoration/build; no restored-image pass is
reused. Owned task registrations are removed after Ready/result0 checks and
the native PC is idle. No human or review-server obligation remains.

The [durable handoff](../_checkpoints/BPR-1971/handoff.md) binds exact scope,
commands, source/images, both native controls and remaining release gates.
This issue supplies focused source-fix acceptance; final common-source
compilation and native release/trust qualification remain the coordinator's
separate obligations.
