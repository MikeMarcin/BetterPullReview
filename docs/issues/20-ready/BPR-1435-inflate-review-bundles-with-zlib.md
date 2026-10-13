---
schema: "implement-rfc.card.v1"
id: "BPR-1435"
title: "Inflate review bundles with zlib and enforce the core library allow-list"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Inflate review bundles with zlib and enforce the core library allow-list

The functional core's review-bundle reader
([review_cycle_zip.cpp](../../../src/core/src/review_cycle_zip.cpp)) inflated
other people's bundles with a hand-written port of puff and a hand-written
CRC table; its only fuzzing replayed 19 seeds. On 2026-10-13 the owner chose
zlib, and allowed core to link a short list of pinned, pure libraries (glaze,
utf8proc, zlib, fmt) instead of hand-writing such systems.

## Acceptance

- AC1: The core links pinned zlib 1.3.2 (raw-inflate and checksum sources only,
  `Z_PREFIX` so its symbols never collide with a system zlib), and the
  hand-written inflater and CRC table are gone. zlib appears in the release
  inventory with its notice and provenance.
- AC2: Inflation keeps its contract: exactly the declared size, the final block
  reached within the input, more output or a truncated or corrupt stream
  refused, bytes after the final block ignored.
- AC3: `cmake/VerifyCoreBoundary.cmake` refuses any core link outside the
  owner's allow-list, and the architecture document and agent instructions
  state the list and the ask-first rule.
- AC4: The Qt-free core graph configures offline with SQLite and zlib cached.

## Evidence

Pending.
