---
schema: "implement-rfc.card.v1"
id: "BPR-1358"
title: "The core SHA-256 runs at about 110 MB/s, one byte per call"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"performance","found_by":"bpr-main-linux, BPR-1349's cost breakdown, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1358--BPR-1358--8b1a9e3ee97d.json","related_cards":["BPR-1349","BPR-1094"]}
acceptance: [{"criterion":"BPR-1358:known-answers","outcome":"pass","revision":"8c8364d64","evidence":{"kind":"command","detail":"core_sha256: FIPS 180-4 vectors, 14 boundary lengths, 1 MiB, every 200-byte split and uneven chunks; 160 digest-using rows pass; three update mutations fail it."}},{"criterion":"BPR-1358:throughput","outcome":"pass","revision":"8c8364d64","evidence":{"kind":"command","detail":"i9-10900KF, GCC 15 -O3: 219 -> 289 MB/s on 64 MiB; 20,015 x 400 B 41.1 -> 32.4 ms; application_feedback_counter_costs first observation at 10k 90.1 ms (93.5 after BPR-1349). Outcome corrected to the portable 1.3x by the coordinator."}},{"criterion":"BPR-1358:no-dependency","outcome":"pass","revision":"8c8364d64","evidence":{"kind":"command","detail":"Portable C++23 in src/core only; core boundary rows and architecture_boundaries pass."}}]
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

The core hash is faster by block-wise processing within the same pure,
dependency-free, portable core implementation. Its output is byte-for-byte
identical. *Corrected 2026-10-09 (coordinator): this card first said "several
times faster". Portable scalar SHA-256 is bound by its compression function
at about 12 cycles per byte (about 300 MB/s on this machine), so block-wise
processing gives about 1.3×. Several times would need hardware SHA
instructions through intrinsics, which the coordinator declined, since a
refresh no longer hashes ([[BPR-1349]]).*

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

## Built and verified (bpr-main-linux, 2026-10-09)

**Change.** `sha256_stream::update` used to push every byte through
`push()`. It now tops up a partly filled block, hashes whole 64-byte blocks
straight from the input through `transform(std::span<const std::uint8_t, 64>)`,
and buffers only the tail. `finish()` and the compression function are
unchanged. The code is portable C++23 with no new include beyond
`<algorithm>`, and no dependency. A second rewrite of the round loop
(uninitialized schedule, constant bounds, reduced choice and majority) gained
nothing measurable and was not kept.

**Digests identical.** The new row `core_sha256`
(`tests/core/sha256_tests.cpp`) passes:
- FIPS 180-4 / NIST: empty, "abc", the 448-bit and 896-bit messages, and one
  million "a".
- A deterministic pattern at 0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128,
  129 and 1000 bytes, and 1 MiB. The expected digests come from Python's
  `hashlib`.
- Every split point of 200 bytes across two `update` calls, 1 MiB fed in
  uneven chunks, and an empty update.

All 160 rows that use these digests pass in `build/dev`: core, idempotency
keys, GitHub mutation and recorded-fixture contracts, review cycle, setup
destination, agent launch, writing delivery, designated authority, SQLite,
tour CLI, counters and the round-trip contract. The one exception was
`application_review_cycle_desktop_public_path`, red on main from BPR-1342 and
fixed separately in `eae64dd67`.

**Controls** (each built, run and reverted):
- The whole-block loop advancing 63 bytes fails "the 896-bit message".
- The top-up stopping one byte short fails "a split at byte 1".
- Leaving a full final block buffered aborts on the out-of-range buffer.

**Throughput** (i9-10900KF, WSL2 Ubuntu 26.04, GCC 15.2.0 `-O3`, standalone
harness over the core source, medians of 7):

| Input | Before | After |
|---|---|---|
| 64 MiB in one call | 306.7 ms, 219 MB/s | 232.4 ms, 289 MB/s |
| 20,015 bodies of 400 B (BPR-1349's 10k shape) | 41.1 ms | 32.4 ms |

`application_feedback_counter_costs` in the optimized tests-on tree (Release,
idle, `-j1`), first observation at 10k: 90.1 ms median (93.5 ms after
BPR-1349). It is 8.3 ms at 1k. Unchanged refreshes hash nothing, as BPR-1349
left them (`bodies_hashed=0`).

macOS and Windows runs of `core_sha256` are owed.
