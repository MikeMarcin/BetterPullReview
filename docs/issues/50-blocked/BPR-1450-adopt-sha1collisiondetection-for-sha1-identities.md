---
schema: "implement-rfc.card.v1"
id: "BPR-1450"
title: "Adopt sha1collisiondetection for every SHA-1 Git identity"
status: "Blocked"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
blocked_by: "owner-core-library-approval"
---

# BPR-1450: Adopt sha1collisiondetection for every SHA-1 Git identity

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`adopt-sha1dc`.

## Blocker

RFC Q1 is unanswered: may sha1collisiondetection (MIT) join core's approved
libraries, as its own pinned static C target linked only by
`better_pull_review_core`, configured as Git builds it? Until the owner
answers Accept, nothing here starts and nothing is added to
`cmake/VerifyCoreBoundary.cmake`. On Reject, follow the RFC's fallback (sha1dc
in an adapter behind a host port for external bytes; core keeps first-party
SHA-1 and the report states the remaining gap) and see [[BPR-1452]].

## Acceptance (after Accept)

- **AC1 (RFC AC02):** Zero mismatches across the known answers, differential
  runs, the full repository sweep and every existing digest-using row.
- **AC2 (RFC AC03):** Every collision input is refused, never digested, at core
  identity construction, host blob verification (`blob_matches`), transport
  verification and Review Cycle publication, with a reason code naming
  collision detection, while the plain-SHA-1 control collides; ordinary
  objects and valid pack checksums are still accepted.
- **AC3 (RFC AC06):** `core-only` and `dev` build; `architecture_boundaries`
  admits exactly the approved target; the first-party `sha1.cpp` is deleted
  and its known-answer rows stay.
- **AC4 (RFC AC07):** Pinned archive and extracted-source digests,
  `PROVENANCE.md`, the licence, a `dependencies.json` entry, notices, SPDX and
  corresponding source; offline configure works; the macOS package audit
  passes. Architecture text, core `AGENTS.md`, the release inventory and
  `sha1.hpp`'s security wording are updated.
