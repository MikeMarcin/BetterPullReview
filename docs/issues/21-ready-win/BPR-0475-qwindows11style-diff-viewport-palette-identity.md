---
id: BPR-0475
title: QWindows11Style diverges from the diff course's viewport palette-identity check
status: Ready
rfc: RFC-BPR-0005
depends_on: []
---

# QWindows11Style diverges from the diff course's viewport palette-identity check

Carded 2026-08-28 from [[BPR-0474]]'s target-native high-DPI course. The
diff-workbench course's viewport-grab palette-identity check
(`src/app/main.cpp:1759` at `7160c283`: the commonest pixel of the pane
viewport grab must equal `QPalette::Base` exactly) fails under Qt's default
Windows style with `DIFF_INTERACTION_FAILURE:appearance-raster-palette` and
`PUBLIC_PATH_FAILED`, while every other exercised style passes.

Measured on the designated Windows 11 x64 host at `7160c283`:

- Fails under default `QWindows11Style` on the native `windows` QPA at real
  125%, 150%, and 200% display scaling — and at 1.0x offscreen with
  `QT_STYLE_OVERRIDE=windows11`, so the divergence is independent of QPA,
  scale, and DPI, and reproducible under matrix conditions (the offscreen
  run also spams `OpenThemeData() failed for theme 16` on stderr).
- Passes: offscreen default (QFusionStyle); native
  `QT_STYLE_OVERRIDE=fusion` full course at real 125%/200%; native
  `windowsvista` full course at 125%; and the default-style native run
  without the interaction probe reports `RENDER_OK` at both scales.
- This is not an unreadable or wrongly painted pane. From the native
  default-style full-window renders, the unchanged-code crop is the applied
  profile base `#0d1117` at 83.3% (125%) and 83.8% (200%) with readable
  profile text `#e6edf3`; the expected palette stamps are base `#0d1117`,
  highlight `#28518c`, window text `#e6edf3` (from-profile, all runs). At
  200% a `#1c2026` fill appears at 4.7% — a Windows11-style control fill
  and the prime candidate for the check's divergent commonest color in the
  viewport-only grab. The exact commonest pixel the failing check saw is
  not emitted on failure and needs an instrumented message.

What this card owns, under RFC-BPR-0005's integrated presentation-polish
and native visual-acceptance scope:

1. Instrument the check's failure message to print the observed commonest
   pixel beside the expected `QPalette::Base`.
2. Root-cause whether QWindows11Style paints a control fill that the
   applied appearance profile should own (product palette/paint handling in
   the workbench viewport) or whether the exact commonest-pixel identity
   assertion needs region or tolerance refinement for style-owned chrome.
3. Fix the product or refine the check accordingly, keeping fusion,
   windowsvista, and offscreen behavior green, and re-run the default-style
   native course on the designated host.

Non-goals: this is not a high-DPI or geometry defect and does not reopen
RFC-BPR-0004 — [[BPR-0474]]'s real 1.25x/2.0x course evidence stands
(fusion full course green, default-style `RENDER_OK`, readable panes,
real-DPR component assertions). Investigation needs a Windows build:
`QWindows11Style` exists only there, and the 1.0x offscreen repro makes it
debuggable under matrix conditions on that host.
