---
schema: "implement-rfc.card.v1"
id: "BPR-1562"
title: "Markdown reader: fenced code coloured by language, as GitHub does"
status: "Ready"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-01","AM2-02","AM2-05"],"related_cards":["BPR-1563","BPR-1564","BPR-1543","BPR-1544","BPR-1558","BPR-1559"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design","landing_after":["BPR-1543","BPR-1544","BPR-1559"]}
---

# BPR-1562: Markdown reader: fenced code coloured by language, as GitHub does

Under [RFC-BPR-0036 owner amendment 2](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-2--2026-10-16-fenced-code-as-github-draws-it),
a fenced block whose info string names a language GitHub highlights is
coloured by the review panes' classifier and colours. Today every fenced
block is plain monospace.

## Acceptance

- **AC1 (AM2-01):** against GitHub's Markdown API rendering of
  `tests/fixtures/markdown/code-blocks-v1/languages.md`, every block GitHub
  highlights whose language the packaged definitions cover is coloured, and
  every block GitHub leaves plain stays plain; tokens wear the panes' role
  colours in the four themes at 1.0, 1.25 and 2.0; indented code, inline
  code, unknown languages and no info string are unchanged.
- **AC2 (AM2-02):** colour arrives asynchronously with scroll position,
  selection, height and text unchanged; one classification per block text
  and definition, later readers served from the cache; a theme change
  recolours without classifying; no fetch; limits respected (negative
  controls).
- **AC3:** the fence scanner's fuzz campaign is clean; the reader's existing
  rows pass; gates (clang-format 22, tidy on touched files, documentation,
  composition contracts resealed for `src/app`); full dev suite once before
  landing.
- **AC4 (AM2-05), owner:** Accept/Reject on a frozen build beside GitHub in
  Light and Dark.

## Coordination

The panes' classification lane (`appearance_application_host`, which
[[BPR-1558]] is repairing) is not touched: the reader uses its own
`syntax_classification_application_host` over the same packaged
classifier, definitions and appearance effect lane, as the Review Cycle's
code views do ([[BPR-1161]]). [[BPR-1559]]'s classifier fix reaches the
reader with no change here. Landing waits for [[BPR-1543]], [[BPR-1544]] and
[[BPR-1559]] on main; this branch is stacked on BPR-1544's.
