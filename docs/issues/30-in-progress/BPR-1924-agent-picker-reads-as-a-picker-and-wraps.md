---
schema: "implement-rfc.card.v1"
id: "BPR-1924"
title: "The agent/model/effort picker reads as a picker, not a text field, and wraps to a second row when the drawer is narrow"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-07, BPR-1914 v004 look","related_cards":["BPR-1878","BPR-1914"]}
acceptance: []
---

# BPR-1924: The agent/model/effort picker reads as a picker, not a text field, and wraps to a second row when the drawer is narrow

## Owner (2026-11-07)

"this box around the model picker looks more like a text entry field than a
dropdown/picker. It also makes me think that maybe the picker needs to wrap to
a 2nd row on the agent tab if the drawer is narrow."

The owner's two screenshots are in the BPR-1914 v004 attended session
(`/Users/jaeger/.codex/attended-review/bpr1914-v004-QBUboY/state`).

## Change

Load `frontend-design:frontend-design` first.

- The bottom bar's agent · model · effort control (Agent tab, and the Tour
  tab's copy from BPR-1878; side chat composers too, if they show it) reads
  unmistakably as a picker: a chevron or popup affordance, and a hover and
  press state, not a text-field-like bordered box around the question field.
  Keep the question field visually distinct from the picker.
- In a narrow drawer, the picker wraps to its own row under the question
  field instead of crowding or truncating it. In a wide drawer it stays on one
  row.

## Acceptance

- AC1. Offscreen pictures at 380, 520 and 900 px drawer widths, light and
  dark, show the picker as a picker. At narrow widths it sits on its own row
  with its full text.
- AC2. A Qt test checks the wrap threshold, and that the picker never
  overlaps or truncates the question field.
- AC3. Owner look: Accept/Reject on a frozen build.
- AC4. The documentation gate passes.
