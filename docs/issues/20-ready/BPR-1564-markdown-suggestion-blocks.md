---
schema: "implement-rfc.card.v1"
id: "BPR-1564"
title: "Markdown reader: suggestion blocks shown as GitHub's Suggested change"
status: "Ready"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-04","AM2-05"],"related_cards":["BPR-1562","BPR-1563","BPR-1212","BPR-1084"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design"}
---

# BPR-1564: Markdown reader: suggestion blocks shown as GitHub's Suggested change

Under [RFC-BPR-0036 owner amendment 2](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-2--2026-10-16-fenced-code-as-github-draws-it),
a `suggestion` block in a published line comment reads as GitHub's
**Suggested change**. GitHub's own view was read from a review on
MikeMarcin/BPR-Test#80: the commented lines all as removed rows, then the
block's lines all as added rows (no context rows), both numbered from the
first commented line, changed words marked only on equal-count pairs, the
code coloured by the file's language.

## Acceptance

- **AC1 (AM2-04):** a current single-line, multi-line, inserting and deleting
  suggestion show GitHub's rows, numbers and word marks; an outdated thread,
  a thread on another file or on the base side shows the added rows only; a
  description, issue comment or review summary shows plain code; there is no
  apply action.
- **AC2:** gates and the reader's rows, as on [[BPR-1562]].
- **AC3 (AM2-05), owner:** Accept/Reject beside GitHub in Light and Dark.
