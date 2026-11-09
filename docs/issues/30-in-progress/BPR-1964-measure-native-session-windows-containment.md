---
schema: "implement-rfc.card.v1"
id: "BPR-1964"
title: "Measure the native-session wrapper's Windows containment"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"test","found_by":"Current-main native qualification registry has an explicitly unmeasured Windows row","mandate":"Owner release-blocker fixes land through an implementer on main; coordinator authorized current-main private Windows build and exact native-session registration/pin measurement on 2026-11-09.","milestone_plan":{"id":"bpr-1964-native-session-boundary","path":"docs/rfcs/plans/bpr-1964-native-session-boundary/milestone-plan.json"}}
---

# BPR-1964: Measure native-session Windows containment

## Outcome and acceptance

- AC1. A fresh, independent Windows source/object store builds the current
  invocation composition, canary and network sandbox targets with their normal
  dependencies. Bind the exact source, compiler, images, wrapper and native
  Limited Interactive context; preserve frozen producers and their evidence.
- AC2. Exercise the same six original wrapper courses uncontained and inside
  the maintained network sandbox. Retain actual results, both streams,
  native exits and cleanup. The original 400-second outer watchdog and
  180-second subprocess limits, workloads, assertions and environment remain.
- AC3. Replace the unmeasured registration using the actual result: remove the
  Windows escape when contained, or retain an exact measured refusal and its
  binding when containment refuses. Change only this case's registration and
  registry entry; run focused manifest/documentation checks and land on main.

## Scope and boundaries

Only `tests/end_to_end/CMakeLists.txt` and
`scripts/verify_test_network_isolation.py` may change test behavior, and only
after actual measurement. The wrapper already adopts the contained temporary
directory factory from BPR-1949. Its six courses and semantic checks remain.

The new engineering checkout is `D:/bpr-native-session-main-r1/src`, exact
source `c4f1de0b0b1c3ee940d5ae08be5f02e34c2ee46b`. It has its own object store,
no Git alternates and no hardlinked packs. Build only the normal invocation
composition, canary and sandbox targets plus their transitive dependencies,
using the existing pinned cache and approved VS18/x64 Release configuration.
Native execution uses a new Limited Interactive task, qualified temporary
root, serial execution and the existing physical capacity guards.

This engineering measurement cannot qualify build43, the Mac-only build44
preview, any final native graph, signing, trust or publication. Preserve all
other failed and unreached Windows courses. The owner's separate Qt decision
changes no first-party functional or containment requirement.

## Native result and main integration

The fresh current-main candidate `8396d01c3` (accepted 8f plus BPR-1967)
passes the original six-course wrapper in 45.94 seconds. The same image and
wrapper fail at the first settings-store prerequisite inside the AppContainer;
later contained checks are unreached. A separate exact-token/handle probe
records WinError 5 opening the first retained storage ancestor with the
production attributes/control flags. The data handle itself resolves in
both DOS and NT form, so the pin names `ancestor-control-denied`.

The normal Windows manifest gate rejects the original unmeasured marker
(CTest 8 / 1.71 seconds) and passes with the measured entry (0 / 1.71 seconds).
The explicit Windows registration, all other pins and every course argument,
oracle, environment entry and 400/180-second limit remain. Source `fd39ea354`
and the evidence packet are pushed through main `107d34683`, preserving all
allocations. Format 22, 74 isolation contracts, 67 documentation contracts,
documentation structure and scoped board validation pass.

The [shared handback](../_checkpoints/bpr1964-1967-native-windows/handoff.md)
binds exact sources/images, retained failures, streams and operator guard
reconciliations. This is measured engineering evidence and a registry repair;
full common-source native qualification and other release obligations remain.
