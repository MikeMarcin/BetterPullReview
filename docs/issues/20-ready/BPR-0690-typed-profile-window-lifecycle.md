---
schema: "implement-rfc.card.v1"
id: "BPR-0690"
title: "Introduce typed BPR profile and window lifecycle"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0594"]
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
