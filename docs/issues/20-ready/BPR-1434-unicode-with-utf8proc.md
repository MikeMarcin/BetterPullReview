---
schema: "implement-rfc.card.v1"
id: "BPR-1434"
title: "Fold paths and decode UTF-8 with utf8proc instead of hand-written tables"
status: "Ready"
rfcs: []
depends_on: ["BPR-1435"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Fold paths and decode UTF-8 with utf8proc instead of hand-written tables

The 2026-10-13 audit found about 1,400 lines of pasted Unicode 16 tables in
[review_cycle_path_folding.cpp](../../../src/core/src/review_cycle_path_folding.cpp),
covering only Latin, Greek and Cyrillic, with a decoder that read lead bytes
F5-FF as three-byte sequences (so `F5 80 80` met `E5 80 80`), and about 17
separately written UTF-8 decoders and encoders, two of which wrote invalid
UTF-8 for a lone surrogate escape. The owner chose utf8proc, one of the
approved pure core libraries.

## Acceptance

- AC1: Core links pinned utf8proc 2.12.0 and exposes one toolkit,
  `bpr/core/utf8.hpp`, for decoding, validation, encoding, canonical
  decomposition and one-to-one case folding. utf8proc appears in the release
  inventory with its notice and provenance.
- AC2: Collision keys decompose and fold every script (kana, Hangul, σ/ς, the
  Kelvin sign, combining-mark order), never apply full foldings such as ß to
  ss, keep invalid bytes as themselves, and never let an invalid sequence meet
  a valid one.
- AC3: The duplicated core decoders (review submission, profile window,
  writing validation, review-cycle JSON, workspace, material, fuzzy match and
  the source matcher) and the agent-stream helpers use the shared toolkit with
  their existing signatures. Deliberately different decoders stay: the
  metered diff validator, the lenient suggestion editor and the Windows WTF-8
  pair.
- AC4: The MCP context helper and the designated live observer refuse a lone
  surrogate escape instead of writing invalid UTF-8, and the observer checks
  raw string bytes.

## Evidence

Pending.
