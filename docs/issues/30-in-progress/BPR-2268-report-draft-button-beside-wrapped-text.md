---
schema: "implement-rfc.card.v1"
id: "BPR-2268"
title: "A report finding's Draft comment keeps its size beside wrapped text"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2268--BPR-2268--e1939a2571ad.json","origin":"owner, 2026-11-18","related_cards":["BPR-2219","BPR-2231"]}
---

# A report finding's Draft comment keeps its size beside wrapped text

## Origin

The owner, 2026-11-18: "Draft comment button in a report doesn't look good
when the description text to its right line wraps".

## Scope and acceptance

- **AC-a.** The button keeps its natural size, its top on the text's first
  line, at every width; the text wraps cleanly beside it.
- **AC-b.** A Qt test at a narrow width with wrapped text asserts the
  button's height and its top against the text's first line.
