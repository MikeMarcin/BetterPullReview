---
schema: "implement-rfc.card.v1"
id: "BPR-0693"
title: "Scope credentials and remembered bindings to BPR profiles"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690"]
acceptance: [{"criterion":"Profile credential and remembered binding isolation with Default-compatible adoption","outcome":"pass","revision":"d0c9b738cda2e2626c6abb4f1d8876776265f4fd plus BPR-0693 finish paths","evidence":{"kind":"observation","detail":"Host/binding2/2pass0.48s; repaired binding1/1pass0.50s; shared authority isolation0.38s; macOS injected contract0.37s. Real Windows VS18 and UbuntuWSL GCC15.2/libsecret0.21.7 isolated platform contracts compiled and executed exit0. Native vaults not touched. Independent medium review preflight finding repaired with limits/cancellation before SQL and unchanged-byte hostile regression. Scoped tidy/format/diff pass. Task proof archives/Windows+WSL roots consumed and removed. Runtime composition remains later0695."}}]
---

# Scope credentials and remembered bindings to BPR profiles

## Scope

Add explicit BPR profile scope to credential authority and GitHub host admission,
remembered-binding metadata and profile-bound native vault factories. Default
keeps existing access/refresh namespaces and 574-byte authority codec as its
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
Implementation is present in the owned application, SQLite and native vault surfaces.
Focused host/binding contracts passed 2/2 in 0.48s; macOS injected vault contract
passed in 0.37s. Shared authority/context and profile-isolation contract passed
in 0.38s. Canonical changed-surface formatting and tidy passed for available
macOS translation units; Windows cross-syntax checks passed with GCC 16.2.

The current native adapter candidate was compiled and its injected contract
executed successfully on Windows NT 10.0.26200 with Visual Studio 18 and Ubuntu
WSL with GCC 15.2/libsecret 0.21.7. Each isolated executable used the platform
adapter, credential material implementation, manual executor and contract test;
these are actual platform executions, not live vault/service acceptance. Both
returned exit 0. Existing remote station checkouts were untouched.

One independent astra/medium review found preflight reads before SQLite limits;
the correction installs connection-local size/SQL/column/opcode and bounded
cancellation handling before any schema read. Hostile 2 MiB fields in v1/v2
and cancellation during quick_check refuse with unchanged database bytes.
Focused binding recheck passed 1/1 in 0.50s and source/test tidy passed. No
additional profile-isolation finding was reported.

Task-created proof roots: local `/tmp/bpr-0693-native-contract-source.tar`,
Windows `C:\Users\mikem\AppData\Local\Temp\bpr-0693-native-contract-20260908`,
and WSL `/tmp/bpr-0693-linux-contract-20260908`. Build/test processes exited, outputs were consumed, and all three proof roots
were removed. No user checkout or credential store was modified.
