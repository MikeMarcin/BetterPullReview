---
schema: "implement-rfc.card.v1"
id: "BPR-1617"
title: "Use deterministic platform SHA-256 in core"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"hashing-replacement-r4","reservation":"docs/issues/_reservations/BPR-1617--BPR-1617--8031beb5bea5.json","milestone_plan":{"id":"deterministic-platform-sha256-v1","path":"docs/rfcs/plans/deterministic-platform-sha256-v1/milestone-plan.json"}}
---

# BPR-1617: Use deterministic platform SHA-256 in core

Owner mandate, 2026-10-18: “Let's amend BPR's architecture to allow deterministic
platform crypto in core, then use CommonCrypto on Mac and CNG on Windows with
HACL as the portable fallback.”

## Outcome

Amend RFC-BPR-0080 and the architecture, then select CommonCrypto on macOS,
CNG on Windows and the pinned HACL implementation elsewhere through the existing
core SHA-256 API. Preserve SHA-1 collision detection and every digest identity.

## Acceptance

- **AC1:** Architecture and RFC r4 distinguish semantic determinism from CPU
  dispatch/internal implementation state and permit only the scoped SHA-256
  platform backend; boundary checks keep other OS crypto/effects excluded.
- **AC2:** The public one-shot/stream SHA-256 paths preserve known answers,
  split/empty updates and independent copies. Updates allocate no first-party
  memory; calls remain noexcept. Startup failure selects HACL; an established
  native stream's failure cannot yield an incorrect digest.
- **AC3:** macOS actually invokes CommonCrypto; a portable build runs HACL.
  Focused composition, format, boundary and documentation checks pass.
- **AC4:** CNG ownership, duplication, chunking and failure behavior have
  focused executable contracts. Seek native Windows evidence; unavailable
  native correctness/throughput remain explicitly on BPR-1448 without
  claiming this milestone closes RFC-BPR-0080.
