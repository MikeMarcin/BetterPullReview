---
schema: "implement-rfc.card.v1"
id: "BPR-2268"
title: "A report finding's Draft comment keeps its size beside wrapped text"
status: "Done"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2268--BPR-2268--e1939a2571ad.json","origin":"owner, 2026-11-18","related_cards":["BPR-2219","BPR-2231"]}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"926baf88e","evidence":{"kind":"command","detail":"offscreen renders at 380 px, 100% and 200%, before and after (kept outside the repository): the chip was stretched to the row's height with its words on two lines; after, it keeps one line's height, its top on the words' first line"}},{"criterion":"AC-b","outcome":"pass","revision":"926baf88e","evidence":{"kind":"command","detail":"qt_agent_report_view check_keep_chip: at 300 and 340 px with wrapped words the chip's box equals its one-line height at 1400 px and its words' padding; its words stay on one line and are level with the first line beside it. Fails on the unfixed source at \"Draft comment stays on one line at 300\" (box 119 against 39)"}}]
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

## Root cause

`report_page.cpp`'s `chip` was a bordered cell in the same table row as the
words beside it. When the words wrapped, the row grew taller, so the
bordered cell stretched to the full row height. The cell also shrank, and its
own words wrapped to "Draft / comment".

## Fix

The chip is now a table of its own, in a top-aligned cell, and its words
never wrap, so it keeps its natural size. The words beside it are
top-aligned, with a 5 px top padding that puts their first line level with
the chip's. Draft all uses the same layout. The frontend-design skill was
loaded for this card.
