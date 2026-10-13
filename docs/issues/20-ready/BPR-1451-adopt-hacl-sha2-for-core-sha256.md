---
schema: "implement-rfc.card.v1"
id: "BPR-1451"
title: "Adopt HACL* generated SHA-2 for the core SHA-256"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1451: Adopt HACL* generated SHA-2 for the core SHA-256

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`adopt-hacl-sha2`.

## Owner answer

Q2 answered **Accept** on 2026-10-13: HACL*'s generated SHA-2 joins core's
approved libraries (`BetterPullReview::HaclSha2` in
`cmake/VerifyCoreBoundary.cmake`). This card still depends on [[BPR-1446]]:
the RFC's HACL* adopt rule needs W1 to W3 under the RFC's throughput
conditions, including an idle machine, which BPR-1446's AC4 has not yet met.

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
