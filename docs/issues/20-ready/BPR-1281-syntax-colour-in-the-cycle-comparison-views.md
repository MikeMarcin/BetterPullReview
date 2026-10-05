---
schema: "implement-rfc.card.v1"
id: "BPR-1281"
title: "Syntax colour in the cycle's comparison views (All cycle changes and the rest)"
status: "Ready"
rfcs: ["RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-cycle-bar-floor-v001 (attended session cbd48b91, event 950, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1163","BPR-1157","BPR-1161"],"acceptance":["BPR-1281:history","BPR-1281:colour","BPR-1281:exact","BPR-1281:boundary","BPR-1281:owner-look"]}
---

# BPR-1281: Syntax colour in the cycle's comparison views (All cycle changes and the rest)

## Origin

The owner accepted `BPR-1182-cycle-bar-floor-v001` (RC61-09) with this note,
quoted exactly, and a screenshot of All cycle changes R1 → R2 on
`src/parser.cpp` in plain text:

> Bar width is okay but why did we lose syntax highlighting on all cycle cahnges?

## Outcome

Find whether the comparison view lost its colours or never had them, and
say which with the commits. Then colour every cycle comparison (All cycle
changes, This round, Since my last review and Choose revisions all share
one pane) through the shared classifier and painter path that the source
pane and the share sheet already use: the packaged classifier through the
desktop's syntax host, and `code_sample_painter`'s format ranges and role
colours. No second classifier and no colour chosen outside the role table.

## Acceptance

- **BPR-1281:history.** The cause is recorded with the commits that show it.
- **BPR-1281:colour.** A rendered comparison, through the real composition,
  paints keywords, comments and strings in the role colours on both removed
  and added rows. A negative control without the classification shows plain
  text and fails the check.
- **BPR-1281:exact.** The text, line numbers, signs and row bands are
  unchanged; a stale classification (of other bytes) is never applied.
- **BPR-1281:boundary.** The view rows, the desktop course and
  `architecture_boundaries` pass; the round-trip pin is unchanged.
- **BPR-1281:owner-look.** The owner's Accept/Reject on a frozen build.
