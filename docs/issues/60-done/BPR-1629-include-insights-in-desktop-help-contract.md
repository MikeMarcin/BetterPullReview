---
schema: "implement-rfc.card.v1"
id: "BPR-1629"
title: "Include approved Insights in the desktop Help contract"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"aaff2f11419257be2c4c4de5646e3465cf4bbf8c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9b901a0a83d1016d5e2da3795b1e7fa5249c4b85","evidence":{"kind":"observation","detail":"Exact seven-entry resident Help order matches approved window Help including Insights; all commands disabled windowless, window labels/key parity and both disabled export gestures asserted. No product UX changed."}},{"criterion":"AC2","outcome":"pass","revision":"9b901a0a83d1016d5e2da3795b1e7fa5249c4b85","evidence":{"kind":"command","detail":"clang-format22 passes; native Windows focused qt_profile_management_window passes5.43s at exact9b901. Documentation gate and67 documentation tests passed during milestone; final documentation/diff validation before finish; Mac/Linux full normal reruns retained coordinator-owned."}}]
---

# BPR-1629: Include approved Insights in the desktop Help contract

The coordinator added this bounded repair after the frozen `f6ba8f43e4477f60941dd24cdac4450ca68de911` native normal gate supplied the actual failure. No signing, credentials, remote publication, test skip, security-policy relaxation or new UX is authorized by this card.

## Acceptance

- AC1: Ground the complete resident Help action order in the accepted Insights implementation and update the stale test expectation, preserving exact labels, separators and all actionable commands disabled while no window is open.
- AC2: Preserve product UI and callbacks, format the touched C++ with clang-format22, pass focused available checks and documentation/diff verification, and retain the coordinator-owned exact native Qt integration rerun.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` bind commands, candidates and pending native/release obligations.
