---
schema: "implement-rfc.card.v1"
id: "BPR-1587"
title: "Shared drawer parts: header, card, ownership marks and empty state"
status: "Ready"
rfcs: ["RFC-BPR-0085"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-foundation", "criteria": ["AC01"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1587: Shared drawer parts: header, card, ownership marks and empty state

Build the RFC-BPR-0085 principles as one set of Qt adapter components, so every
later milestone draws from them rather than stock widgets.

## Outcome

- **Drawer header.** The tab row with the accepted Comments tab look (34
  pixels, semibold labels, 2-pixel signal underline, count pill) and tools as
  icons at its right; extracted from today's tray header so the Comments
  drawer is unchanged.
- **Card.** The closed note and the open card (panel, shadow, 3-pixel edge and
  notch, in amber for reading and blue for writing), painted from palette
  roles, with the ownership variants: solid, dashed, dashed with a lock,
  agent square avatar, numbered tour circle.
- **Empty state.** Glyph, one sentence, one action. Waiting and failure use
  [[BPR-1576]]'s pattern when it lands; this card adds no loading look of its
  own.
- **Rules as tests.** A Qt course renders each part in the four themes at 100,
  125 and 200 percent and fails on a nested tab, a stacked button column or a
  colour outside the palette roles.

## Acceptance

- RFC-BPR-0085 AC01 for the parts: the components exist, the Comments drawer
  uses the extracted header unchanged (its existing courses pass), and the
  render course with its negative controls passes.

## Boundaries

Qt adapter only. No surface changes look except through BPR-1588 in the same
milestone. `frontend-design:frontend-design` is loaded first.
