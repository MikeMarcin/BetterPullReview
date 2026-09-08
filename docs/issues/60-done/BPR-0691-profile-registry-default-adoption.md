---
schema: "implement-rfc.card.v1"
id: "BPR-0691"
title: "Persist profile registry and Default adoption"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0690"]
acceptance: [{"criterion":"Bounded durable profile registry, Default adoption, CAS and non-mutating refusal","outcome":"pass","revision":"554564f8331143ce0ca6a4106e9bf1c791268ccf plus BPR-0691 finish paths","evidence":{"kind":"observation","detail":"Focused application_profile_registry and sqlite_profile_registry_storage passed2/2 in0.72s after independent review repair. Read-only limits precede preflight; hostile4MiB fields and10001rows prove bounded work and unchanged stored data. Canonical tidy2changedTUs/format/diff passed. No runtime/root creation/window persistence claim."}}]
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

Implemented versioned SQLite registry and application host with durable-only
acceptance, one pending effect, cancellation/reload and revision CAS. Default
adopts the existing root in place; new root identities derive only from IDs.
Focused host/storage contracts passed 2/2 in 0.72s after the review repair.
Canonical changed-source tidy, formatting and diff checks passed.

Independent astra/medium review identified preflight before read limits. Repaired
by installing read-only length/SQL/column/VM limits immediately after open, with
bounded progress and at most 65 retrieved rows; accepted records sort in memory.
Regressions with 4 MiB fields and 10,001 rows verify bounded work, zero SQL sorts
and unchanged schema/rows/metadata on refusal. No further review findings.
Existing domain stores and native vaults remain untouched. Runtime composition,
root directory creation and window-session persistence belong to later slices.
Temporary standalone outputs and crash fixtures were removed; normal shared
build outputs remain in use.
