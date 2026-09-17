---
schema: "implement-rfc.card.v1"
id: "BPR-0869"
title: "Windows: the device sign-in code is not fixed-pitch at 1.6 times the row's size"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-0653-implement; also in task-20260917-win-hardening","found_at":"20d99914","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0869: Windows: the device sign-in code is not fixed-pitch at 1.6 times the row's size

## Defect

`qt_composed_github_scale_100`, `_125` and `_200` all fail on the Windows
station at the assertion "the code is in a fixed-pitch font at 1.6 times the
row's size", on the device sign-in code field.

Two things make this a Windows-specific font-selection defect rather than a
composition regression:

- It reproduces at `20d99914` with every [[BPR-0653]] change reverted, so it is
  not caused by the Windows title row.
- The same three rows **pass on macOS**: the integrator ran
  `qt_composed_github_scale_100` green at `2e71061d`
  (`build/r2-logs/dsp5-0862-tests.log`), and the Linux full sweep at
  `a1864fa2` did not report them among its eleven failures.

It also appears in this station's earlier `task-20260917-win-hardening` log, so
it has been red on Windows for some time with no card.

The likely shape is font resolution: the assertion asks for a **fixed-pitch**
face at a size ratio of 1.6, and the family the code field asks for is
presumably resolving to a proportional fallback on Windows, or the metric the
ratio is computed from differs there. Confirm which before changing anything —
a request for a family Windows does not have is a different fix from a ratio
computed off the wrong row height.

## Scope

- On the Windows station, report what the code field's resolved font actually
  is: the requested family, the resolved family, `QFontInfo::fixedPitch()`, and
  the point and pixel sizes of both the code field and the row it is measured
  against, at 100%, 125% and 200%.
- Name whether the product picks the wrong face on Windows (a real visible
  defect: the sign-in code is meant to be unmistakably fixed-pitch so a person
  can read it off the screen and type it) or the assertion measures something
  platform-dependent.
- Fix that side. If it is font selection, prefer naming a fixed-pitch family
  Windows actually ships, with `QFont::Monospace` as the style hint so the
  fallback is still fixed-pitch, rather than pinning one family per platform.
- These are the strip's own composed rows, so check the fix does not disturb
  [[BPR-0653]]'s Windows title row or the macOS and Linux results, which are
  green.

## Acceptance

- `qt_composed_github_scale_100`, `_125` and `_200` pass on Windows, and still
  pass on macOS and Linux.
- The card names whether the product's font selection regressed or the
  assertion was platform-dependent.
- The rendered Windows sign-in code is shown to be fixed-pitch at the intended
  size, not only asserted in a view-model test.
