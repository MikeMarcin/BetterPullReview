---
schema: "implement-rfc.card.v1"
id: "BPR-2273"
title: "A long draft comment grows to fit, then scrolls with the wheel and trackpad"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2271--BPR-2273--d45a84d10eb3.json","found_by":"owner, 2026-11-18","related_cards":["BPR-1212","BPR-1290","BPR-1775"]}
---

# BPR-2273: A long draft comment grows to fit, then scrolls with the wheel and trackpad

## Observed

Owner, 2026-11-18: "A long draft comment neither scrolls with 2 finger
MacBook gestures (scroll wheel) nor expands to fit the contents. It has to
do one or the other".

## Acceptance

- AC1. The drawer's comment editors grow with their text up to about 60%
  of the drawer's height, then scroll inside.
- AC2. A wheel or two-finger scroll over such an editor moves it, by angle
  or by pixel delta; at its end the drawer takes the rest. No parent
  swallows it.
- AC3. A closed long draft is never clipped silently: it clamps with an
  ellipsis and opens in full.
- AC4. Qt tests: a long editor grows to its maximum and then has a vertical
  range; synthetic wheel events with angle and with pixel delta move it; a
  closed long draft is not clipped.
- AC5. Focused tests, format and tidy on the changed files, and the
  documentation gate pass.
