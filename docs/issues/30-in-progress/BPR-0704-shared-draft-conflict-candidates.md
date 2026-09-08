---
schema: "implement-rfc.card.v1"
id: "BPR-0704"
title: "Share revision-aware drafts and retain conflicting editor candidates"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690", "BPR-0697"]
---

# Share revision-aware drafts and retain conflicting editor candidates

## Scope

Introduce pure revision-aware draft edit/discard/publish admission and a bounded
profile/review-scoped application owner for accepted draft allocation/state.
Window consumers retain local candidates separately from shared accepted drafts.
A stale edit or publish returns a typed conflict with current accepted state;
no mutation request may follow refused publication admission. Keep exact profile,
account, review and window lifecycle identity, bounded text/records/consumers and
consumer-aware shutdown. Existing mutation grants and terminal authority remain.

Qt presents accepted state and rejected candidate separately, preserving exact
local text across shared refresh and late completion. Explicit reload/reapply
resolves a conflict; presentation alone cannot silently replace the candidate.
Use this owner in the current application facade, with actual same-profile
multi-window construction supplied in later runtime integration. This card does
not add durable editable drafts or change publishing permissions.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Worker owns draft core/application/Qt and focused tests. Primary owns app wiring,
CMake, acceptance and bookkeeping; parallel epoch/ledger work is preserved.

## Acceptance

Two consumers editing revision N produce one accepted N+1 and one preserved
conflicting candidate. Stale discard/publish and retired/foreign consumers cannot
change accepted state or submit effects. Shared refresh, selection changes and
late terminals cannot erase rejected candidate bytes. Explicit conflict resolution
uses the latest accepted revision. Existing draft and exact publication contracts
remain passing, with a composed editor/publication path using the new owner.

## Evidence

Read-only inspection found the current application increments a draft revision
without edit admission, while Qt replaces differing accepted body text. Shared
editable draft persistence remains outside this bounded slice. No implementation
or new acceptance test has run.
