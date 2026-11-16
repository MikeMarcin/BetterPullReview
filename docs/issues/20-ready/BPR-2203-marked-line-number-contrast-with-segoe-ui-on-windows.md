---
schema: "implement-rfc.card.v1"
id: "BPR-2203"
title: "On Windows at 125%, the open thread's first line number reaches the light theme's text floor with Segoe UI 9 pt"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2200's Windows Qt run with the fonts loaded, Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2201--BPR-2203--f448700c62dd.json","related_cards":["BPR-2200"]}
---

# BPR-2203: On Windows at 125%, the open thread's first line number reaches the light theme's text floor with Segoe UI 9 pt

## Found

BPR-2200 made every Windows Qt course run offscreen with the Windows fonts
and Segoe UI 9 pt. With them, `qt_diff_workbench_scale_125` fails on the
marked line numbers. With no fonts it fails earlier, on the open-thread bar.
Lane `claude/v0-5-0-prerelease-targets-52265d` at `18a70bb96`, Release:

    marked-number light plain line 2: ground #f0f2f5 ink #5b6570 ratio 5.28
    marked-number light line 11 (open thread's first line): ground #ebe2cf ink #5e6772 ratio 4.45
    FAILED: light: every marked line number reaches the theme's text floor on its tinted cell

Every other marked row reaches the floor (4.53 to 4.8). On line 11 the
darkest painted pixel is `#5e6772`, lighter than the theme's gutter ink
`#5b6570`. The number "11" is two thin stems, and at 125% in Segoe UI 9 pt
they may never cover a whole pixel. If so, the painted number reads below
the floor even though its ink colour meets it on that ground.

Undecided: whether the product should darken the ink on this tint (so the
painted number meets the floor), or the measure should account for
antialiased coverage. This is an accessibility floor: do not lower it.

## Scope and acceptance

- With Segoe UI 9 pt on Windows at 125%, every marked line number reaches
  the light theme's text floor as painted, or the card records with
  evidence why the measure, not the product, is at fault and corrects it.
- `qt_diff_workbench_scale_125` passes on the Windows station with the
  fonts loaded, and the diff workbench courses still pass on macOS and Linux.

## Reproduce on the Windows station

As BPR-2200's card describes. Build
`test_better_pull_review_qt_diff_workbench_tests`, then run
`QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.25 BPR_EXPECTED_QT_SCALE=1.25 better_pull_review_qt_diff_workbench_tests.exe`
and read the `marked-number` lines.
