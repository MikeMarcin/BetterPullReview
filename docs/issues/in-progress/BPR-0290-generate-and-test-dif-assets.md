---
id: BPR-0290
title: Generate and mechanically verify Dif sprite assets
status: In Progress
rfc: RFC-BPR-0019
depends_on: [BPR-0288, BPR-0289]
---

# Generate and mechanically verify Dif sprite assets

Generate the committed strips, descriptor, and digest inventory, then register
focused contract tests that prove reproducibility and reject every malformed or
over-budget pack relevant to RFC-BPR-0019.

## Required implementation

- Generate one-row 256 x 256 RGBA strips with exactly one transparent gutter
  cell at each end and no unreferenced content frames.
- Emit a compile-time catalog sharing the manifest schema version and digest,
  with no filesystem paths or runtime JSON parser.
- Compare two fresh regenerations with the committed generated tree byte for
  byte, including file names and absence of extras.
- Test alpha, edge clearance, baseline/body/center travel, returns, priorities,
  representative frames, descriptions, loop seam, handoff, rates, dimensions,
  frames, decoded pixels, widths, and file-byte limits.
- Register one offline asset contract in both developer and core-only CTest
  selection without adding Qt or art-generation dependencies to core-only.

## Completion evidence

- Focused positive and hostile contract tests pass and are non-vacuous.
- Clean regeneration produces zero diff from committed generated assets.
- The descriptor compiles in a narrow consumer and rejects stale manifest or
  resource digests.
