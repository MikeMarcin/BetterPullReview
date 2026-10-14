---
schema: "implement-rfc.card.v1"
id: "BPR-1451"
title: "Adopt HACL* generated SHA-2 for the core SHA-256"
status: "Done"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9a7235228","evidence":{"kind":"command","detail":"144 digest-using rows pass (all core_*, application feedback/epoch/review-cycle/mutation, all sqlite_*, workspace_source, github mutation and publication contracts); stored-digest rows (core_sha256, core_git_objects, core_review_cycle_retrieval/round/material/bundle, sqlite_review_mutation_recovery_storage, sqlite_epoch_catalog_storage_contract, github_mutation_contract, review_cycle_capture/candidate, application_writing_delivery) pass unchanged; evaluation driver correctness/git/sweep256 zero mismatches.","path":"docs/issues/60-done/BPR-1451-adopt-hacl-sha2-for-core-sha256.md"}},{"criterion":"AC2","outcome":"pass","revision":"9a7235228","evidence":{"kind":"observation","detail":"sha256_hacl.cpp implements the noexcept sha256_stream API with HACL* block-level calls only (init, update_nblocks <=1 GiB, update_last, finish); the allocating streaming API is never called; core_sha256 split-point and uneven-chunk rows pass.","path":"docs/issues/60-done/BPR-1451-adopt-hacl-sha2-for-core-sha256.md"}},{"criterion":"AC3","outcome":"pass","revision":"9a7235228","evidence":{"kind":"command","detail":"dev and core-only configure and build; core_dependency_boundary, core_boundary_contract, architecture_boundaries (new HACL* single-owner rule) and architecture_boundary_contract pass; src/core/src/sha256.cpp deleted, core_sha256 kept.","path":"docs/issues/60-done/BPR-1451-adopt-hacl-sha2-for-core-sha256.md"}},{"criterion":"AC4","outcome":"pass","revision":"9a7235228","evidence":{"kind":"command","detail":"BprHaclSha2Provider pins archive and licence digests; third_party/hacl-star LICENSE and PROVENANCE.md; dependencies.json entry; NOTICE_FILES; dependency_modernization_contract (offline configure), package_runtime_contract, release_security_audit_contract and binary_dependency_audit_contract pass on macOS; release inventory updated.","path":"docs/issues/60-done/BPR-1451-adopt-hacl-sha2-for-core-sha256.md"}}]
---

# BPR-1451: Adopt HACL* generated SHA-2 for the core SHA-256

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`adopt-hacl-sha2`.

## Owner answer

Q2 answered **Accept** on 2026-10-13: HACL*'s generated SHA-2 joins core's
approved libraries (`BetterPullReview::HaclSha2` in
`cmake/VerifyCoreBoundary.cmake`). The owner waived the idle-machine
condition for the macOS measurements the same day; [[BPR-1446]] is done.

## Acceptance

- **AC1 (RFC AC02):** Zero mismatches across the known answers, differential
  runs and every existing digest-using row; data persisted before adoption
  still loads and matches.
- **AC2:** The `noexcept` `sha256_stream` contract holds through HACL*'s
  block-level calls with no per-update allocation.
- **AC3 (RFC AC06):** `core-only` and `dev` build; `architecture_boundaries`
  admits exactly the approved target; the first-party `sha256.cpp` is deleted
  and `core_sha256` stays.
- **AC4 (RFC AC07):** Packaging as for any adopted library (provenance,
  licence, `dependencies.json`, notices, SPDX, corresponding source, offline
  configure, macOS package audit).

## Built and verified (macOS arm64, 2026-10-13)

Implementation `9a7235228` on main `94fcc9e5b`.

**Library.** [`BprHaclSha2Provider.cmake`](../../../cmake/BprHaclSha2Provider.cmake)
acquires HACL* `504c2987` (archive SHA-256 `e32f7add…c52c`, 10,228,408 bytes;
`dist/LICENSE.txt` digest checked; offline honoured; only `dist/` re-extracted)
and builds `better_pull_review_hacl_sha2` / `BetterPullReview::HaclSha2` from
`Hacl_Hash_SHA2.c` (`gcc-compatible`, `msvc-compatible` under MSVC). Core links
it privately. `src/core/src/sha256.cpp` is deleted; `sha256_hacl.cpp`
implements the unchanged `bpr/core/sha256.hpp` API through
`Hacl_Hash_SHA2_sha256_init`, `_update_nblocks` (at most 1 GiB per call),
`_update_last` and `_finish`: eight state words, one block, a byte count, no
allocation, `noexcept`. No HACL* header reaches core's public headers.
`architecture_boundaries` keeps `Hacl_` symbols and the SHA-256 constants to
that file; its contract test covers the rule. `third_party/hacl-star` holds
the MIT `dist` licence and provenance (the top-level Apache-2.0 licence covers
only the F* sources, none compiled).

**Checks.** 144 digest-using rows pass, including every `core_*` row,
`application_feedback_counter_costs`, the epoch host, review cycle and
review-mutation rows, every `sqlite_*` row, `workspace_source`,
`github_mutation_contract` and `github_publication_contract`, plus
`application_review_round_trip_composition_contract` (no sealed file changed),
the tour CLI ordinary rows and `application_release_maintenance`. Rows that pin
stored digests pass unchanged: `core_sha256` (22 digests), `core_git_objects`,
`core_review_cycle_retrieval`, `_round`, `_material`, `_bundle`,
`sqlite_review_mutation_recovery_storage`,
`sqlite_epoch_catalog_storage_contract`, `github_mutation_contract`,
`review_cycle_capture`, `review_cycle_candidate`,
`application_writing_delivery`. Contracts: `core_dependency_boundary`,
`core_boundary_contract`, `architecture_boundaries`,
`architecture_boundary_contract`, `dependency_modernization_contract` (offline
configure), `package_runtime_contract`, `release_security_audit_contract`,
`binary_dependency_audit_contract`, the three `hash_evaluation_*` rows. The
evaluation driver (correctness, Git in both formats, `main` as SHA-256) reports
zero mismatches. `core-only` configures and builds; format and changed-line
clang-tidy are clean. MSVC (`/sdl` on the generated C) is untested here.
