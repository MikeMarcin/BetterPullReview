---
schema: "implement-rfc.card.v1"
id: "BPR-1971"
title: "Wait for both agent history loads before enabling Send"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"kind":"bug","found_by":"aa7 Windows original native-session wrapper engineering","mandate":"Coordinator amendment r4 approves the confirmed seven-path readiness correction, unchanged guards/oracles and current-source Windows/Mac distinguishing proofs on2026-11-09.","milestone_plan":{"id":"bpr-1971-native-launch-admission","path":"docs/rfcs/plans/bpr-1971-native-launch-admission/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"970812bfe2a1107625c788a2e87d2a220e4ba93d","evidence":{"kind":"artifact","detail":"Same sealed controlled r3 counterexample on Windows and Mac: actual legacy Loaded false, native ready, Send enabled, unchanged process starts, immediate unique saved-conversation-still-loading refusal. Holds released after38/15ms before original failing outcome waits; original failures/inconclusive attempts/unreached sixth retained. Exact temporary sources restored normally."}},{"criterion":"AC2","outcome":"pass","revision":"970812bfe2a1107625c788a2e87d2a220e4ba93d","evidence":{"kind":"artifact","detail":"Both callbacks combine readiness through existing refresh; scalar is_loaded avoids history copying, original body condition/1965 return/Stop/host guard preserved. Exact current1972-inclusive source: Mac six-course positive0/21.420s, old-UI negative8/21.073s; Windows positive0/46.953s, old-UI negative8/41.635s at exact deterministic readiness assertion, no standalone negative wrapperPASS. Original400/180 limits and unsupported/private_storage_failure/no-start/no-fallback oracles unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"2d0c3265d2262f713c60c58af70e29f01c6eeb57","evidence":{"kind":"artifact","detail":"Seven tested code hashes merged and origin verified on main2d0; c763/owner1972 policy and NEXT1974 preserved. Windows host default0/0.312s and real263169-entry course0/140.377s at original180s, all XMLs actual pass not skip. Format22, two production TU tidy22, architecture, normal reviewed142 source audit, documentation and explicit three-criterion binding pass. Both platform variants restored/normal rebuilt; all forests empty, exact owned tasks Ready/result0 removed, native PC idle and original43 task unchanged. Separate final common release gates remain explicit."}}]
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
