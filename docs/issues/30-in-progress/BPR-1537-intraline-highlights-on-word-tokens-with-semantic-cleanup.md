---
schema: "implement-rfc.card.v1"
id: "BPR-1537"
title: "Intraline highlights on word and punctuation tokens with semantic cleanup"
status: "In Progress"
rfcs: ["RFC-BPR-0082", "RFC-BPR-0004"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15 (relayed by the coordinator)","owner_note":"I feel like the quality of intra-line diffs has gotten a lot worse from a human perspective since we changed algorithms.","governing_rfc":"RFC-BPR-0082","related_cards":["BPR-1481","BPR-1480","BPR-1485","BPR-1467"],"reservation":"docs/issues/_reservations/BPR-1537--BPR-1537--945227dd29b1.json"}
---

# BPR-1537: Intraline highlights on word and punctuation tokens with semantic cleanup

The owner reported on 2026-10-15: "I feel like the quality of intra-line diffs
has gotten a lot worse from a human perspective since we changed algorithms."
[[BPR-1481]] moved a similar pair's intraline ranges from the engine's
budgeted character LCS table to dtl's O(NP) comparison over the pair's Unicode
scalars ([RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md)).
Both are minimal character edits with no cleanup; RFC-BPR-0082's candidate
table already names "semantic cleanup" as the way to change what is
emphasized, and its intraline row makes that a look-and-judge criterion.

This card diagnoses what a reader sees, then computes intraline ranges over
word, number, whitespace and punctuation tokens with dtl as the sequence
engine (still per line only), cleans the result up so small incidental
equalities between edits do not split the emphasis, and falls back to
emphasizing the whole changed extent when the pair shares too little. Spans
stay on UTF-8 boundaries of the original line
([RFC-BPR-0004 §9.2](../../rfcs/RFC-BPR-0004-diff-computation-line-mapping-grouping.md)),
pairing is unchanged, and the core stays pure, deterministic and metered.

## Acceptance

- **AC1, diagnosis:** A corpus of realistic changed line pairs (renames,
  inserted arguments, string edits, reordered tokens, whitespace and
  punctuation changes, mostly rewritten lines; from repository fixtures and
  this repository's history) is run through the pre-switch engine, the current
  one and the candidate, side by side, and the human-visible failure classes
  are named with examples.
- **AC2, engine:** Intraline ranges come from dtl's common tokens of the pair,
  followed by a deterministic cleanup and a low-similarity fallback. Every
  span starts and ends on a token boundary of valid UTF-8; the cost stays
  counted on the engine's step meter and bounded; dtl stays in
  `src/core/src/diff_intraline.cpp`; no new library.
- **AC3, tests:** Corpus golden tests pin the candidate's spans, with a
  negative control that the character-level output fails; the existing diff
  and intraline suites, `core-only`, the affected `dev` tests, clang-format 22,
  tidy on changed lines and the documentation gate pass.
- **AC4 (human):** The owner accepts, on a frozen build and offscreen renders
  of the old, current and candidate algorithms: "intraline highlights read
  better than before".

## Implementation

Core only: `diff_compute.cpp`'s `intraline_ranges` and
`diff_intraline.cpp`. Qt painting is unchanged (other cards own diff paint
for selection visibility). Never drive or capture the owner's desktop; the
owner looks at the frozen build themselves.
