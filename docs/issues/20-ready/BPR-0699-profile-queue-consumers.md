---
schema: "implement-rfc.card.v1"
id: "BPR-0699"
title: "Share one profile queue with bounded window consumers"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
---

# Share one profile queue with bounded window consumers

## Scope

Keep one queue host/store and refresh owner per profile, with bounded explicit
window projection attachment, replay and detach. Preserve the existing composed
constructor/path while allowing a profile runtime to outlive any initial window.
Only one profile-level observer evaluates notification transitions; subscriber
fan-out must not duplicate notifications, refreshes or writes. Context includes
exact profile/window generation and retired consumers receive no late results.
Queue schema v6 remains the durable authority; no storage migration is needed.
Actual multi-window Qt composition and notification activation-window selection
remain later runtime work.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, bookkeeping and acceptance. Workers have
exclusive queue or workbench/storage paths and preserve parallel changes.

## Acceptance

Two consumers observe one accepted queue mutation and one refresh/notification
observation. Initial replay, reentrant detach, stale/foreign attachment, bounded
capacity, repeated close/open and last-consumer shutdown preserve exact ownership.
Existing queue host/storage contracts remain passing. The shipped facade uses
the fan-out implementation; one composed queue path verifies compatibility.

## Evidence

Read-only ownership inventory established this bounded split. No implementation
or new acceptance check has run for this card. BPR-0697/0698 extraction logs and
finish decisions were consumed and removed; shared build outputs remain.
