---
schema: "implement-rfc.card.v1"
id: "BPR-1452"
title: "Hash SHA-1 block-wise if sha1dc is not adopted"
status: "Done"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
acceptance: [{"criterion":"DISPOSITION: conditional Q1-Reject fallback is inapplicable","outcome":"pass","revision":"2b09f74f0287b1bd0a282b40cb31805a0973d293","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1452/verification.md","detail":"Q1 Accept (2026-10-13) selected BPR-1450, whose accepted sha1dc implementation remains composed; sha1.cpp is absent. The authored plan says only the branch matching Q1 runs. Original AC1-AC3 retained with not-applicable dispositions; no fallback implementation, correctness, throughput or boundary-test result is claimed. RFC-0080 native/station/Q3/package and closeout obligations remain open."}}]
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

## Execution record

Closed as **inapplicable after the Q1 Accept**, 2026-10-18, under mandate
`owner-ready-backlog-20261018-r2` and the owner's instruction to reassess older
cards for relevance. This is a conditional branch disposition, not completion
of a first-party SHA-1 optimization.

The owner answered Q1 **Accept** on 2026-10-13. [[BPR-1450]] adopted
sha1collisiondetection and removed `src/core/src/sha1.cpp`. The authored
`adopt-sha1dc` milestone says "only the card matching the answer runs";
the RFC already records that this card is not needed after Q1 Accept.
At reconciliation revision `2b09f74f0287b1bd0a282b40cb31805a0973d293`, core
still composes `sha1_collision_detecting.cpp`, links `BetterPullReview::Sha1dc`
and refuses `SHA1DCFinal` detections. The public SHA-1 header and wrapper
are byte-identical to BPR-1450's accepted revision `23c6b345f`.

| Original conditional criterion | Disposition |
|---|---|
| AC1: fallback digest correctness | Not applicable: the first-party fallback was not selected. No fallback correctness run is claimed. |
| AC2: fallback before/after throughput | Not applicable: no fallback was implemented or timed. The evaluation's historical control-arm measurements stay historical. |
| AC3: no new dependency and core boundary rows | Not applicable to the unselected fallback. The selected sha1dc dependency and its acceptance remain on BPR-1450. |

The original acceptance text above is retained. The terminal record accepts
this branch disposition only. No product, build, package, public API or
persisted identity changes. RFC-BPR-0080 remains In Progress; [[BPR-1448]],
[[BPR-1449]], conditional Q3 and [[BPR-1453]] retain their native, performance
and full closeout obligations. The authored plan, imported source and immutable
RFC baseline are unchanged.

See [verification](../_checkpoints/BPR-1452/verification.md) and
[continuation](../_checkpoints/BPR-1452/continuation.md).
