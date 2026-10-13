---
schema: "implement-rfc.card.v1"
id: "BPR-1451"
title: "Adopt HACL* generated SHA-2 for the core SHA-256"
status: "Blocked"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
blocked_by: "owner-core-library-approval"
---

# BPR-1451: Adopt HACL* generated SHA-2 for the core SHA-256

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`adopt-hacl-sha2`.

## Blocker

RFC Q2 is unanswered: may HACL*'s generated SHA-2 C (MIT) join core's approved
libraries, as its own pinned static target linked by core? Until the owner
answers Accept, nothing here starts and nothing is added to
`cmake/VerifyCoreBoundary.cmake`. On Reject, core keeps its first-party
SHA-256 and this card closes with that decision recorded.

## Acceptance (after Accept)

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
