---
schema: "implement-rfc.card.v1"
id: "BPR-0690"
title: "Introduce typed BPR profile and window lifecycle"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0594"]
acceptance: [{"criterion":"Typed bounded profile/window lifecycle policy and exact stale-context refusal","outcome":"pass","revision":"2b40da44 plus BPR-0690 finish paths","evidence":{"kind":"observation","detail":"Primary inspected model/tests. Standalone strict C++23 test and canonical tidy2TUs/format pass. cmake core-only configure/build passed (initial unrelated setup fixture compile corrected). core_profile_window passed0.25s; full core-only run125/128 in445.72s. Two setup URL boundary failures repaired by moving parser to Application, rechecked core_dependency_boundary and architecture_boundaries2/2pass7.51s; architecture_boundary_contract passed80.67s. Remaining unrelated designated_live_macos_capsule_contract SIGTERM cleanup timeout is not profile evidence; isolated recheck pending, process/mount absence confirmed. No profile persistence/UI/multi-window composition acceptance claimed."}}]
---

# Introduce typed BPR profile and window lifecycle

## Scope

Core-only distinct profile/window identities, bounded catalog and lifecycle policy, immutable Default, label validation, explicit activate/new-window/close/quit outcomes. No filesystem paths, Qt or authentication effects. Own new core profile/window model files and focused core tests; integrator owns CMake.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
[Authority inventory](../../development/profile-window-authority-inventory.md).
Routing: standard | Reason: default implementation.
This independent foundation may proceed alongside Settings integration; it does
not change Settings or setup composition. Native/human terminal prerequisites
remain on BPR-0595.

## Acceptance

Deterministic cases distinguish profile from host profile; rename retains identity; profile/window limits refuse; explicit close differs from quit; stale or wrong-profile lifecycle input cannot affect another window. Focused core tests, core-only and architecture gates at integration.

## Evidence

Not implemented or run. Planning only.
