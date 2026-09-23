---
schema: "implement-rfc.card.v1"
id: "BPR-0869"
title: "Windows: the device sign-in code is not fixed-pitch at 1.6 times the row's size"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-0653-implement; also in task-20260917-win-hardening","found_at":"20d99914","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"AC1 qt_composed_github_scale_100, _125 and _200 pass on Windows, and still pass on macOS and Linux","outcome":"pass","revision":"1eab866e","evidence":{"kind":"command","detail":"Windows (bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial): with the fix all three pass in two runs (37.7-42.5 s), with qt_device_authorization_scale_100/125/200 and qt_workbench_title_row_scale_100/125/200; without it (same tree) scale_100 fails at 'the code is in a fixed-pitch font at 1.6 times the row's size'. macOS and Linux: not run from this station; the fallback runs only when QFontInfo reports the requested face proportional, and those platforms' rows were green on this same assertion (QFontInfo(code).fixedPitch()) before the change, so their font is unchanged.","path":"docs/issues/60-done/BPR-0869-the-device-sign-in-code-field-is-not-fixed-pitch-at-1-6-times-the-row-on-windows.md#cause-offscreen-on-windows-the-fixed-fonts-name-matches-arial-bpr-main-win-2026-09-23"}},{"criterion":"AC2 The card names whether the product's font selection regressed or the assertion was platform-dependent","outcome":"pass","revision":"1eab866e","evidence":{"kind":"observation","detail":"Platform-dependent, not a regression: on the real windows platform systemFont(FixedFont) is Courier New and resolves fixed-pitch; on offscreen (the rows' platform) it is 'monospace', which the font database over C:/Windows/Fonts matches, with the Monospace hint ignored, to Arial. The product relied on the hint, so the fix is a fixed-pitch fallback in the product.","path":"docs/issues/60-done/BPR-0869-the-device-sign-in-code-field-is-not-fixed-pitch-at-1-6-times-the-row-on-windows.md#cause-offscreen-on-windows-the-fixed-fonts-name-matches-arial-bpr-main-win-2026-09-23"}},{"criterion":"AC3 The rendered Windows sign-in code is shown to be fixed-pitch at the intended size, not only asserted in a view-model test","outcome":"pass","revision":"1eab866e","evidence":{"kind":"artifact","detail":"The composed rows render the whole sign-in and assert QFontInfo on the live code field at 1.6 times the row's point size; their rasters (station copies build/logs/bpr-0869-main-win/rasters/) show WDJB-MJHT in Arial without the fix and in a monospaced face with it at 100% and 200%. The panel's font code run on the windows platform (no window shown) resolves Courier New, fixedPitch 1, 14.25 pt / 19 px for 14.40 requested."}}]
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

## Windows run at the merged head, 2026-09-17 (bpr-win, task-20260917-win-merged-head-runs)

The head is `2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`. The full row-by-row sweep
(362 rows, 309 passed, 53 failed) is `docs/verification/dsp4-windows-merged-head-sweep.md`.

Still red at the merged head: `qt_composed_github_scale_100`, `_125` and `_200`
all fail at `FAILED: the code is in a fixed-pitch font at 1.6 times the row's
size` (30.2 s, 31.6 s, 38.2 s in the sweep).

## Cause: offscreen on Windows, the fixed font's name matches Arial (bpr-main-win, 2026-09-23)

**The assertion was platform-dependent; the product's selection did not
regress. But the product relied on a style hint that one font database
ignores, so the fix is in the product.**

A standalone probe (station helper, not committed) ran the panel's own font
code, `systemFont(FixedFont)` with `QFont::Monospace`, `setFixedPitch(true)`
and ×1.6, under both Windows platforms with Qt 6.11.2:

| Platform | `systemFont(FixedFont)` | Resolves to | `fixedPitch()` |
| --- | --- | --- | --- |
| `offscreen` (the test rows; `QT_QPA_FONTDIR=C:/WINDOWS/Fonts`) | `monospace` | **Arial** | **0** |
| `windows` (the real desktop; no window was shown) | `Courier New` | Courier New, 14.25 pt / 19 px for 14.40 requested | 1 |

On the desktop the product already shows a fixed-pitch code. The offscreen
platform names the fixed font `monospace`. Its font database over
`C:\Windows\Fonts` has no such family and no fontconfig alias, and it ignores
the `Monospace` hint, so the rows rendered the code in Arial. Named
explicitly, `Consolas`, `Cascadia Mono` and `Courier New` all resolve
fixed-pitch there.

**The fix** (`src/adapters/qt/src/device_authorization_panel.cpp`,
`apply_fonts`): when the requested face does not resolve fixed-pitch, the
code font falls back, in order, to the platform's family and then `Cascadia
Mono`, `Consolas`, `Courier New`, `Menlo`, `DejaVu Sans Mono` and `Liberation
Mono`. Where the platform's fixed font already resolves fixed-pitch (the
Windows desktop, macOS, Linux with fontconfig), nothing changes. Offscreen on
Windows the code now resolves to Cascadia Mono at the same 1.6× size.

**Rendered.** The composed rows write the sign-in they check. In the station
copies under `build/logs/bpr-0869-main-win/rasters/` (not committed), the
code `WDJB-MJHT` is in Arial without the fix (`negative-100-…`), and in a
monospaced face with it (`fix-100-…`, `fix-200-…`).

**Rows, `build/dev` (MSVC 19.51 Debug), offscreen, serial:**

- Without the fix (the panel file at HEAD, same tree),
  `qt_composed_github_scale_100` fails at "the code is in a fixed-pitch font
  at 1.6 times the row's size" (34.21 s).
- With the fix, in two runs, all nine rows pass each time:
  - `qt_composed_github_scale_100`, `_125` and `_200` (37.7–42.5 s);
  - `qt_device_authorization_scale_100`, `_125` and `_200` (5.0 s);
  - [[BPR-0653]]'s `qt_workbench_title_row_scale_100`, `_125` and `_200`
    (14.1–18.0 s).

`clang-format --dry-run --Werror` (22.1.8) is clean on the file.
macOS and Linux were not run from this station. They take the fallback only
if their fixed font resolves proportional, which their green rows show it
does not.
