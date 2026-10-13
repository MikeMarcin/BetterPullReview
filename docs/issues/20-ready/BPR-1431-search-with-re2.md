---
schema: "implement-rfc.card.v1"
id: "BPR-1431"
title: "Search with RE2 instead of a hand-written regex engine"
status: "Ready"
rfcs: []
depends_on: ["BPR-1432"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1431--BPR-1431--e2ce4383ccc3.json","origin":"owner request 2026-10-13 after the RFC-0077 critical review","follows":"RFC-BPR-0077"}
---

# Search with RE2 instead of a hand-written regex engine

RFC-BPR-0077 shipped Find and Find in Files on a hand-written Thompson VM,
a KMP literal path and 6,460 lines of generated Unicode tables in core. The
2026-10-13 critical review confirmed a silent false negative: with Regular
Expression and Whole Word, `a.*` over `ba a` returned a complete empty result
instead of `[3,4)`. The owner asked to replace the engine with a library and to
remove the hand-written code.

## Acceptance

- AC1: Pane Find and Find in Files match on pinned Google RE2 through core's
  `source_regex_engine` interface, implemented by a new `regex` adapter and
  supplied by the composition root. Core links no library; RE2 and Abseil are
  pinned, hash-verified archives with notices, provenance and inventory entries.
- AC2: The hand-written VM, KMP path, Unicode tables, their generator and
  `third_party/unicode` are removed.
- AC3: Whole Word resumes a refused match at the next start whose previous
  scalar is not a word scalar, so the review's counterexample and similar
  cases find their whole words. Lookaround, backreferences and `\C` stay
  refused before compilation.
- AC4: The existing core, host, public-path and performance courses pass on the
  new engine, and the committed performance thresholds still hold.

## Evidence

Pending.
