---
schema: "implement-rfc.card.v1"
id: "BPR-1450"
title: "Adopt sha1collisiondetection for every SHA-1 Git identity"
status: "In Progress"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1450: Adopt sha1collisiondetection for every SHA-1 Git identity

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`adopt-sha1dc`.

## Owner answer

Q1 answered **Accept** on 2026-10-13: sha1collisiondetection joins core's
approved libraries (`BetterPullReview::Sha1dc` in
`cmake/VerifyCoreBoundary.cmake`). The owner also accepted the collision
amendment and waived the idle-machine condition for the macOS measurements
(RFC-BPR-0080, 2026-10-13); [[BPR-1446]] is done.

## Acceptance

- **AC1 (RFC AC02):** Zero mismatches across the known answers, differential
  runs, the full repository sweep and every existing digest-using row.
- **AC2 (RFC AC03, amended 2026-10-13):** The SHAttered and SHA-mbles inputs
  are detected and refused, never digested, when hashed as raw byte streams
  through the core SHA-1 stream and one-shot functions, while the plain-SHA-1
  control collides. Code inspection and tests show that core identity
  construction, host blob verification (`blob_matches`), transport
  verification and Review Cycle publication all use that detecting library
  and end a detection at a refusal with a reason code naming collision
  detection. Ordinary objects and valid pack checksums are still accepted.
- **AC3 (RFC AC06):** `core-only` and `dev` build; `architecture_boundaries`
  admits exactly the approved target; the first-party `sha1.cpp` is deleted
  and its known-answer rows stay.
- **AC4 (RFC AC07):** Pinned archive and extracted-source digests,
  `PROVENANCE.md`, the licence, a `dependencies.json` entry, notices, SPDX and
  corresponding source; offline configure works; the macOS package audit
  passes. Architecture text, core `AGENTS.md`, the release inventory and
  `sha1.hpp`'s security wording are updated.

## Built and verified (macOS arm64, 2026-10-13)

Implementation `23c6b345f`, merged with main and re-verified at the finish
revision.

**Library.** [`BprSha1dcProvider.cmake`](../../../cmake/BprSha1dcProvider.cmake)
acquires sha1collisiondetection `a06f1adb` (archive SHA-256 `b9c89e19…1729`,
787,039 bytes, licence digest checked, offline honoured, re-extracted) and
builds `better_pull_review_sha1dc` / `BetterPullReview::Sha1dc` from
`lib/sha1.c` and `lib/ubc_check.c` as Git configures them. Core links it
privately. `src/core/src/sha1.cpp` is deleted; `sha1_collision_detecting.cpp`
implements `bpr/core/sha1.hpp` (`sha1_result = expected<sha1_digest,
sha1_refusal>`, the 2,400-byte context held in place, no allocation).

**Refusal on every SHA-1 identity path (AC2, inspection).**

| Point | Call chain | Detection ends at |
|---|---|---|
| Core identity construction | `git_blob_identity`, `build_git_trees`, `git_commit_identity`, `verified_commit_tree` → `git_object_digest` → `git_object_hasher::finish` → `sha1_stream::finish` (`SHA1DCFinal`) | `git_object_refusal`, `git_tree_error`, `git_commit_error`, `verified_commit_error` `::collision_detected` |
| Host blob verification | `epoch_application_host` `blob_matches` → `core::git_object_hasher` | `blob_check::collision_detected` → `diff_blob_status::failure` |
| Transport verification | `gh_cli_git_smart_transport` `blob_identity` → `core::git_blob_identity` (`QCryptographicHash::Sha1` removed) | `git_smart_transport_failure::collision_detected`; a batch answer is left out |
| Review Cycle publication | candidates (`build_git_trees`, `git_commit_identity`), `plan_outgoing`, `review_cycle_publisher`, `github_publication_session` | `candidate_limit`, `outgoing_refusal`, `cycle_publish_limit` `::collision_detected`; the session refuses |

`architecture_boundaries` allows `QCryptographicHash::Sha1` only in the pack
checksum (`installed_git_cache.cpp`) and refuses any other SHA-1 API outside
`sha1_collision_detecting.cpp`; `architecture_boundary_contract` proves both
refusals. `core_git_objects` refuses the SHAttered 320-byte prefixes and the
SHA-mbles pair through the stream and one-shot functions while their plain
SHA-1 collides (`f92d74e3…`, `8ac60ba7…`; fixtures with their source in
`tests/fixtures/sha1-collisions`). The diff observation has no reason field,
so the host's `collision_detected` ends at `diff_blob_status::failure`.

**Checks.** Full `dev` build. 47 focused rows pass after merging main:
`core_git_objects`, `core_sha256`, every `core_review_cycle*` row,
`sqlite_review_cycle_storage`, `application_epoch_host`,
`review_cycle_publication`, `review_cycle_publication_matrix`,
`review_cycle_git_readers`, `github_publication_contract`,
`native_git_smart_transport_contract`, the three `hash_evaluation_*` rows,
`core_dependency_boundary`, `core_boundary_contract`,
`architecture_boundaries`, `dependency_modernization_contract` (offline
configure), `package_runtime_contract`, `release_security_audit_contract`,
`binary_dependency_audit_contract` and
`application_review_round_trip_composition_contract` (no sealed file changed);
`architecture_boundary_contract` and the format check pass; changed-line
clang-tidy reports nothing. The evaluation driver (correctness, Git, sweep)
reports zero mismatches with the core arm now sha1dc, across 107,084
repository objects with no detection. Windows and Linux builds are not run here.
