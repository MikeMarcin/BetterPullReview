---
schema: "implement-rfc.card.v1"
id: "BPR-1435"
title: "Inflate review bundles with zlib and enforce the core library allow-list"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"396f23f95","evidence":{"kind":"observation","detail":"Core links pinned zlib 1.3.2 (Z_PREFIX, raw-inflate/checksum only, confined to review_cycle_zip_codec.cpp); hand-written inflater and CRC removed; inventory, notice and packaging contracts pass.","path":"docs/issues/60-done/BPR-1435-inflate-review-bundles-with-zlib.md"}},{"criterion":"AC2","outcome":"pass","revision":"396f23f95","evidence":{"kind":"observation","detail":"core_review_cycle_bundle adds truncated, empty, trailing-bytes and oversize cases; pack oracle reads real bundles; all 103 core suites pass.","path":"docs/issues/60-done/BPR-1435-inflate-review-bundles-with-zlib.md"}},{"criterion":"AC3","outcome":"pass","revision":"396f23f95","evidence":{"kind":"observation","detail":"VerifyCoreBoundary refuses core links outside glaze/utf8proc/zlib/fmt; core_boundary_contract pins accepted and refused cases; docs and AGENTS state the list.","path":"docs/issues/60-done/BPR-1435-inflate-review-bundles-with-zlib.md"}},{"criterion":"AC4","outcome":"pass","revision":"396f23f95","evidence":{"kind":"observation","detail":"dependency_modernization_contract configures the Qt-free graph offline with SQLite and zlib and still refuses it without SQLite.","path":"docs/issues/60-done/BPR-1435-inflate-review-bundles-with-zlib.md"}}]
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

- AC1: `cmake/BprZlibProvider.cmake` builds zlib 1.3.2 (SHA-256, size and
  license checked; offline cache honoured) from adler32, crc32, inffast,
  inflate, inftrees and zutil with `Z_PREFIX ZLIB_CONST`; core links it
  privately. `review_cycle_zip_codec.cpp` is the only file that includes
  `zlib.h`, so its renaming macros never reach other code.
  `review_cycle_zip.cpp` falls from 587 to 336 lines. `third_party/zlib`,
  `packaging/dependencies.json` and `NOTICE_FILES` carry it;
  `package_runtime_contract`, `release_security_audit_contract` and
  `binary_dependency_audit_contract` pass.
- AC2: `core_review_cycle_bundle` keeps the declared-size, corrupt-stream,
  fixed-Huffman and stored-block cases and adds a stream without a final block,
  an empty final block, bytes after the final block, and output beyond the
  expected size; `core_review_cycle_bundle_pack_oracle` reads the real example
  bundles. All 103 `core_*` suites pass with zlib linked.
- AC3: `cmake/VerifyCoreBoundary.cmake` refuses any `better_pull_review_core`
  link outside glaze, utf8proc, zlib and fmt; `core_boundary_contract` adds
  accepted and refused cases (RE2, Qt, SQLite, Threads). The architecture
  document, root `AGENTS.md` and `src/core/AGENTS.md` state the list.
- AC4: `dependency_modernization_contract` configures the Qt-free graph offline
  with SQLite and zlib cached, and still refuses it without SQLite.
- Gates: `core_dependency_boundary`, `architecture_boundaries`,
  `architecture_boundary_contract` and the documentation gate pass;
  clang-format 22 applied; clang-tidy 22 reports no finding on changed lines.
