---
schema: "implement-rfc.card.v1"
id: "BPR-0691"
title: "Persist profile registry and Default adoption"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690"]
---

# Persist profile registry and Default adoption

## Scope

Application catalog port/host and SQLite versioned bounded profile registry. Default adopts existing root in place once; new profiles receive isolated root identity and standard defaults. No vault-key migration or multi-window composition in this slice. Own new application and SQLite registry files plus focused tests; integrator owns CMake.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
[Authority inventory](../../development/profile-window-authority-inventory.md).
Routing: standard | Reason: default implementation.
This independent foundation may proceed alongside Settings integration; it does
not change Settings or setup composition. Native/human terminal prerequisites
remain on BPR-0595.

## Acceptance

Fresh and legacy Default adoption are idempotent through interrupted transaction/reopen; create/rename retain stable IDs; collection limit and future/malformed storage refuse without cross-profile fallback. Focused host/manual-executor and SQLite migration tests. Existing domain stores remain untouched.

## Evidence

Not implemented or run. Planning only.
