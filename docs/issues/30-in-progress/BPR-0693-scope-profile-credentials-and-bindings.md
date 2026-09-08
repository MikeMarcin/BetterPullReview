---
schema: "implement-rfc.card.v1"
id: "BPR-0693"
title: "Scope credentials and remembered bindings to BPR profiles"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690"]
---

# Scope credentials and remembered bindings to BPR profiles

## Scope

Add explicit BPR profile scope to credential authority and GitHub host admission,
remembered-binding metadata and profile-bound native vault factories. Default
keeps existing access/refresh namespaces and574-byte authority codec as its
canonical storage; non-Default uses new versioned per-profile namespaces with
no legacy lookup fallback. Every vault instance rejects foreign profile keys
before native effects. Tokens stay in existing native vaults.

Migrate device binding schema1 rows transactionally to Default-only schema2;
non-Default cannot adopt schema1. Bind each private store to its expected profile,
preserve exact-account restart /user validation and tombstone/delete ordering.
No new publishing authority, profile UI or multi-window runtime is introduced.

Owned boundaries: application credential/host/device-binding types and tests;
SQLite device-binding adapter/tests; native macOS/Windows/Linux vault factories,
backends and contract tests. Separate workers own separate native platforms;
primary owns shared contracts, CMake, composition integration and acceptance.
Routing: standard | Reason: default implementation.
Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC02/05/08/09 foundation: Default namespace bytes remain unchanged; two new
profile IDs produce disjoint access/refresh slots; foreign profile operations
fail before native backend calls. Host-produced keys always carry admitted
profile and authenticated viewer. Schema1 Default adoption survives rollback
and reopen; future/malformed/foreign stores refuse without fallback. Restart
still requires exact-account validation. Focused host, binding storage and three
native contract selectors; actual platform capability limits remain explicit.

## Evidence

Read-only scope inventory identified one credential-key construction helper,
three immutable native namespace owners and the current schema1 binding store.
In-place Default namespace adoption avoids multi-slot secret promotion; it is
an explicit profile-bound storage choice, not a retrieve-on-miss fallback.
No implementation or new test has run for this slice yet.
