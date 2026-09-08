---
schema: "implement-rfc.card.v1"
id: "BPR-0694"
title: "Resolve private storage roots from admitted profile identities"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0691"]
acceptance: [{"criterion":"Exact registry-derived private root authority and non-mutating refusal","outcome":"pass","revision":"b2416c39c9bee2e328a572f59e73171f85a6ad04 plus BPR-0694 finish paths","evidence":{"kind":"observation","detail":"Application/SQLite root tests2/2pass0.68s onmacOS; build/tidy5TUs/format/diff pass. Whole-registry validation, opaque authority, Default in-place, isolatedIDroots, no labelpaths, symlink/traversal/private-owner checks and untouched existing metadata proven. Medium independent review no findings. WindowsACLnotexecuted; admission-timevalidationnotretainedlease."}}]
---

# Resolve private storage roots from admitted profile identities

## Scope

Application root authority selected only from a valid durable registry entry and a SQLite/filesystem adapter resolving owner-private ID-derived roots. Default retains the canonical base root; isolated profiles use profiles/profile-N. Reject unknown IDs, malformed authority, symlinks/path escape and foreign roots before profile stores are constructed. No labels become paths; no profile runtime or window UI in this slice.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns CMake, shared composition, bookkeeping and acceptance. Workers own
explicitly assigned new authority/adapter/test files and preserve others.

## Acceptance

Focused authority and filesystem contract tests cover Default in place, distinct profile roots, rename invariance, invalid tokens, symlink refusal, permissions and cancellation. Existing domain data must remain untouched.

## Evidence

Immutable authority validates the entire registry and selects an exact ID.
The adapter reuses private-directory primitives; existing directories are never
chmod-repaired, symlinks/traversal fail, and only newly created children receive
private permissions. No state/v1 directory is created under the profiles parent.

Focused application_profile_storage_authority and sqlite_profile_storage_root
passed 2/2 in 0.68s. Build, canonical tidy across five changed translation units,
format and diff checks passed. Tests prove Default bytes preserved, distinct
private roots, rename invariance, untouched symlink targets, unchanged permissive
and foreign directory metadata, and cancellation. Independent medium review
found no blocking defect. Windows native ACL execution was not run for this
slice; shared native primitives are reused. Returned paths are admission-time
validation, not retained filesystem leases. No temporary artifacts remain beyond
normal shared build output; fixtures cleaned their own roots.
