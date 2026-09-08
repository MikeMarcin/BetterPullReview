---
schema: "implement-rfc.card.v1"
id: "BPR-0699"
title: "Share one profile queue with bounded window consumers"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
acceptance: [{"criterion":"All bounded card acceptance criteria","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Core-only queue contracts 6.53s, changed-TU tidy and composed application_queue_composition_contract 3.68s pass. Medium review found two corrected reentrancy failures, both narrowly rechecked. Shared notification observation is singular; actual multi-window runtime remains later."}}]
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

The queue host now admits at most 32 exact-context consumers with monotonic
attachment identities, accepted-state replay and reentrant delivery checks.
One fresh-publication observer remains separate from coalesced window delivery;
legacy combined notification/UI observation preserves all fresh transitions.
Independent medium review found lost reentrant legacy transitions and effects
started after callback shutdown. Both were corrected with discriminating narrow
regressions. Core-only queue tests pass (6.53s); both changed translation units
pass tidy and formatting. No queue storage schema changed.

Core-only queue contracts 6.53s, changed-TU tidy and composed application_queue_composition_contract 3.68s pass. Medium review found two corrected reentrancy failures, both narrowly rechecked. Shared notification observation is singular; actual multi-window runtime remains later.
