---
schema: "implement-rfc.card.v1"
id: "BPR-1596"
title: "On Windows and Linux the title row starts at the edge, with no macOS window-control gap"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "linux", "mac"]
metadata: {"origin":"owner note on BPR-1580's acceptance, 2026-10-17","owner_note":"I also noticed the home button looks misplaced on windows. Seems like it's still offset for the macos window controls.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-0646","BPR-0653","BPR-1129","BPR-1580"],"reservation":"docs/issues/_reservations/BPR-1596--BPR-1596--538e525a145a.json"}
---

# BPR-1596: On Windows and Linux the title row starts at the edge, with no macOS window-control gap

The owner, accepting [[BPR-1580]] on 2026-10-17: "I also noticed the home
button looks misplaced on windows. Seems like it's still offset for the
macos window controls."

Governing design: the title row of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and
RFC-BPR-0052 ([[BPR-1129]]): the platform inset, then (Windows only,
[[BPR-0653]]) the compact menu control, then Home. On macOS the inset is the
78 points the traffic lights take ([[BPR-0646]]); elsewhere it is nothing,
and on Windows the caption buttons end the row at the right.

## Acceptance

- AC1: The title row's leading inset is one named per-platform value:
  78 on macOS, 0 on Windows and Linux. A test checks the value for each
  platform (simulated, so every platform's answer is checked on any host) and
  that the window built on the running platform uses its own value, with Home
  the first control after it (after the menu control on Windows) and, on
  Windows, the caption buttons last.
- AC2: Nothing at the row's start is an invisible gap: the Windows menu
  control's glyph is repainted in the live palette when the theme changes,
  as Home's and the other strip glyphs are, so it can never draw in the old
  ink on the new ground. (The source already gives Windows and Linux no
  inset; a menu glyph left in the previous theme's ink reads as an empty
  space before Home, which is the likeliest cause of what the owner saw.)
- AC3: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.

## Later platform look

Not an acceptance criterion (the coordinator, 2026-10-17: land once tests
prove it). The Windows station can confirm visually that Home sits after the
menu control at the window's left edge in Light and Dark, with the caption
buttons at the right; record it here when it runs.

## Implementation

Qt presents: `workbench_strip` names the per-platform leading inset and
`workbench_window` uses it; the Windows menu control joins
`refresh_toolbar_icons`. No core or host change, no QSS. Never drive or
capture the owner's desktop.
