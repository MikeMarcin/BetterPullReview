---
id: BPR-0118
title: Apply the code line height, or retire the setting
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0116]
---

# Apply the code line height, or retire the setting

`code_line_height()` returns the correct derived measurement and is unit
tested, but no surface consumes it, so the RFC's criterion that line height
"persist and preview immediately" is met only in the first half. BPR-0116
closed the tab-width half of that shortfall and established why this half could
not be closed with it.

## What was measured

`QPlainTextDocumentLayout` ignores line height in every mode Qt offers. A probe
on macOS with Qt 6.11.1 measured the first block's `blockBoundingRect().height()`
in a `QPlainTextEdit` over a three-line document:

| Applied to every block | Measured block height |
|---|---|
| nothing (baseline) | 19 |
| `ProportionalHeight` 200 | 19 |
| `FixedHeight` 40 | 19 |
| `MinimumHeight` 40 | 19 |
| `LineDistanceHeight` 40 | 19 |
| `topMargin` 7 and `bottomMargin` 5 | 19 |

Block margins are ignored as well, so the customary workaround is unavailable.
Nothing reachable from outside the layout changes the block height.

## Why this is a decision and not a task

Honouring the setting means replacing the code panes' text-rendering strategy.
The two candidates each cost something real:

- **`QTextEdit`** honours `ProportionalHeight`, but `QTextDocumentLayout` lays
  out the whole document rather than the visible blocks. RFC-BPR-0005's
  virtualization evidence rests on `QPlainTextEdit` and records 68 window
  widgets at 60, 6,000, and 60,000 lines. Adopting `QTextEdit` puts that
  bounded-widget claim and the large-file figures at risk to satisfy one
  typography setting, and would require re-establishing both.
- **A custom `QAbstractTextDocumentLayout`** keeps the widget but takes on
  painting, hit testing, selection, and scrolling geometry. That is a large
  surface to own correctly, and every part of it is load-bearing for the
  comparison surface's measured axis and ribbons.

A third option is legitimate and should be considered on its merits: **retire
the setting**. A code font's line spacing would then be whatever the font
supplies. That is honest, costs nothing, and removes a control the product may
not need — but it is a product decision about what BetterPullReview offers, not
an implementation shortcut.

Do not close this by making the setting appear to apply. The delivering slice
declined to fake it, and that judgement stands: a setting that looks applied and
is not defeats the next reader's attempt to find the bug.

## Required direct evidence

- Whichever route is chosen, a line-height change visibly alters code line
  spacing on the composed surface and the applied value matches
  `code_line_height()` — or the setting and `code_line_height()` are removed
  together, along with the RFC criterion that names it.
- If the rendering strategy changes, RFC-BPR-0005's bounded-widget and
  large-file figures are re-measured at 60, 6,000, and 60,000 lines and shown
  to still hold, or the claim is restated with the new numbers.
- `measured_geometry_change::line_spacing` becomes reachable from production
  code, or is removed with the setting.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
