---
schema: "implement-rfc.card.v1"
id: "BPR-1452"
title: "Hash SHA-1 block-wise if sha1dc is not adopted"
status: "Backlog"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1452: Hash SHA-1 block-wise if sha1dc is not adopted

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), the
control arm.

## Outcome

The core SHA-1 still feeds every byte through `push()`. The RFC keeps an
optimized first-party SHA-1 as its control arm only: if the owner rejects Q1,
core keeps first-party SHA-1 for local identities, and it should then hash
whole blocks as [[BPR-1358]] did for SHA-256. BPR-1446 measures this arm.
If Q1 is accepted, [[BPR-1450]] deletes `sha1.cpp` and this card is not needed.

## Acceptance

- **AC1:** Byte-identical digests: the CAVP vectors, `core_git_objects`, every
  split point and uneven chunking, and the digest-using rows.
- **AC2:** Measured throughput before and after under the RFC's conditions.
- **AC3:** No new dependency; the core boundary rows pass.
