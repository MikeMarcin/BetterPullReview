---
schema: "implement-rfc.card.v1"
id: "BPR-1450"
title: "Adopt sha1collisiondetection for every SHA-1 Git identity"
status: "Backlog"
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
amendment recorded in the RFC. This card still depends on [[BPR-1446]]: the
RFC's sha1dc adopt rule needs W1 to W3 under the RFC's throughput conditions,
including an idle machine, which BPR-1446's AC4 has not yet met.

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
