---
schema: "implement-rfc.card.v1"
id: "BPR-1358"
title: "The core SHA-256 runs at about 110 MB/s, one byte per call"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"performance","found_by":"bpr-main-linux, BPR-1349's cost breakdown, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1358--BPR-1358--8b1a9e3ee97d.json","related_cards":["BPR-1349","BPR-1094"]}
---

# BPR-1358: The core SHA-256 runs at about 110 MB/s, one byte per call

## Finding

[[BPR-1349]]'s cost breakdown measured the core's `core::sha256` alone, at
`4d850e218` (i9-10900KF, WSL2, GCC 15 `-O3`). It hashed the 8 MB of
comment-body text of a 10k-conversation pull request in 69 to 78 ms, about
110 MB/s, because it processes one byte per call. Before BPR-1349's Stage 1,
that hash was most of every feedback-count refresh. After Stage 1, the first
observation of a pull request still hashes every body.

## Outcome

The core hash is several times faster, by block-wise processing within the
same pure, dependency-free core implementation. Its output is byte-for-byte
identical.

## Acceptance

- Known-answer vectors (FIPS 180-4 / NIST examples, empty input, block
  boundaries at 55, 56, 63, 64 and 65 bytes, and multi-block input) and the
  repository's existing digest fixtures all produce identical digests.
- A measured throughput before and after, with the hardware and build
  recorded. Re-measure `application_feedback_counter_costs`'s first
  observation.
- No new dependency in `src/core`; the core boundary rows pass.

## Boundaries

SHA-256 is a widely used primitive here: idempotency keys, digests, pins and
receipts. Any change must keep every existing digest identical, and is its
own card for that reason (coordinator, 2026-10-09).
