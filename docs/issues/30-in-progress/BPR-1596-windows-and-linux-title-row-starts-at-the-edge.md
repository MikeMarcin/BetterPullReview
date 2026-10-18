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

## Evidence (2026-10-17)

- Finding: the source already gave Windows and Linux no inset (a
  compile-time 0 beside macOS's 78), so a build from this tree draws no
  traffic-light gap. The Windows menu control, which stands first in that
  row, kept the glyph painted at construction: `refresh_toolbar_icons`
  repainted Home and every other strip glyph on a theme change but not this
  one, so after the appearance profile applied its theme the glyph could sit
  in the old ink on the new ground and read as empty space before Home.
- Change: `workbench_strip.hpp` names `title_row_platform`,
  `current_title_row_platform()` and `title_row_leading_inset()` (78 on
  macOS, 0 on Windows and other desktops); the window's inset uses it; the
  Windows menu glyph is repainted with the others. Spec updated.
- Tests: `test_title_row_leading_inset_per_platform` in `--title-row`
  checks every platform's value on any host, the running platform's inset
  width, that the first control (Home; the menu control on Windows) starts
  right after it, and on Windows that the caption buttons end the row and
  that the menu glyph takes the new ink after a theme change (the Windows
  parts compile and run on Windows only). Negative control: an inset 30
  wider fails with "the row's inset is its platform's value: 108".
  `qt_workbench_title_row` 100/125/200 pass; on the merge with main
  `95e900ba4` the title row, focus order, Home discovery, workbench window,
  composed GitHub, documentation rows and the round-trip composition
  contract pass, and `DOCUMENTATION_OK`.
- One full `run_test_suite.py --preset dev` on the branch over main
  `d29a89252`: 766 passed, 8 failed, 1 skipped. Three documentation rows
  need the MASK `.agents` link and pass with it; `live_mutation_isolation`
  passes alone; `application_epoch_host` (an intermittent hang, passing in
  1.6 s on another run) and `application_diff_host` (a 10 ms stop budget)
  are application-layer rows that do not link the Qt adapter; the two
  large-review benchmarks exceed the Mac responsiveness budget under a load
  average near 60. None touches this change.
- clang-format 22 clean; clang-tidy 22 over `workbench_window.cpp` and
  `workbench_strip.cpp`: no findings. No `src/app` change, so no reseal.
- Windows visual look: still open for the Windows station, as noted above.
