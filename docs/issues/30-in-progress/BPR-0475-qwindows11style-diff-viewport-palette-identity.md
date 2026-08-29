---
id: BPR-0475
title: QWindows11Style diverges from the diff course's viewport palette-identity check
status: In Progress
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

## Instrumented — 2026-08-28

Item 1 is portable and landed from the macOS session: on divergence the
check now fails as `appearance-raster-palette observed=<color> share=<p>%
expected=<base> expected-share=<p>%`, tallied over the same viewport grab
it always judged. `application_diff_workbench_public_path` stays green here
(14.33 s) — the passing path is byte-for-byte the old behavior — and the
format gate passes. The JaegAW Remote Control lane now runs the
1.0x-offscreen `QT_STYLE_OVERRIDE=windows11` repro against this build to
name the divergent pixel, its share, and where it concentrates in the grab;
edit authority for item 3 stays with the integrating session.

## Diagnosed — 2026-08-28, evening

The instrumented line, verbatim from the Remote Control lane's run at
`3309687d` in the `windows-lane` worktree (fast-forwarded clean, freshly
linked app launch-probed allowed, no policy refusal):

```
DIFF_INTERACTION_FAILURE:appearance-raster-palette observed=#f6f7f9 share=70.2% expected=#ffffff expected-share=0.0%
```

**`expected-share=0.0%` is the whole diagnosis**: `QPalette::Base` is not
outvoted by intruding chrome — it is absent from the grab. The lane also
measured the passing `windowsvista` course's render inside the located
viewport region: Base 53.07% there versus 0.0% under `windows11`, with
`#f6f7f9` at 6.68% versus 70.2% — a clean inversion. `#f6f7f9` is the app's
own gutter/header chrome family, differing from Base by (9, 8, 6) per
channel, so a tolerance would silently accept a pane not painted with Base
at all. Item 2 therefore resolves to **product paint handling**, and this
supersedes [[BPR-0474]]'s `#1c2026`-control-fill hypothesis. Controls held
at this head: no-override (fusion) passed 13.05 s and `windowsvista` passed
13.19 s; the failing run writes no frames (it aborts before render), and
the `OpenThemeData` stderr spam recorded at `7160c283` is absent at
`3309687d` — a flagged, unexplained delta. Mechanism: QWindows11Style's
polish gives the scroll-area viewport an explicit palette carrying the
style's control fill, which both replaces Base and blocks later profile
propagation.

## Fixed pending target-native proof — 2026-08-28, evening

`source_pane` now keeps its viewport's `QPalette::Base` aligned to its own
palette — the applied appearance profile's — realigning on the viewport's
`PaletteChange` (undoing a style polish's substitution; an equality guard
ends the recursion) and on the pane's own `PaletteChange` (carrying profile
changes past an explicit viewport palette). Where no style substitutes, the
guard makes it a no-op, which the portable evidence confirms: clean build,
format gate green, and the composition-aware focused set —
`qt_composed_github_scale_100/125/200`, `qt_diff_workbench_scale_100/125/200`,
`application_diff_workbench_public_path`,
`application_uiforetw_selected_diff_contract` — passed 8/8 in 103.7 s.
What item 3 still needs is the target-native run: `windows11` course PASS,
with no-override and `windowsvista` controls staying green.

## The palette hypothesis is falsified; the fill attribute is the mechanism — 2026-08-28, late evening

The target-native run at `d4a76956` failed **byte-identically** to the
pre-fix run — `observed=#f6f7f9 share=70.2% expected=#ffffff
expected-share=0.0%` — no reduced residual, no effect at all, while the
no-override and `windowsvista` controls passed and the six-selector Qt
sweep passed target-native. The lane then falsified the mechanism with a
standalone probe (bare `QPlainTextEdit`, same Qt 6.10.3, offscreen,
`windows11`): every `QPalette::Base` — application, window, pane, viewport
— already `#ffffff`, `WA_SetPalette` 0 on pane and viewport alike, both
palette realignments NO-OP, grab unmoved at the style fill. There is no
palette substitution to undo. The real discriminator:
**`autoFillBackground(viewport)` is 0 under QWindows11Style and 1 under
QWindowsVistaStyle and QFusionStyle** — the style clears the fill so it can
paint its own panel, and Base is not overwritten but simply never painted.
The probe demonstrated the remedy: `viewport()->setAutoFillBackground(true)`
makes the grab equal Base under `windows11` and is inert where the fill was
already on.

The correction is landed: the palette-realignment code is removed as
falsified dead weight, and `source_pane` now restores the viewport fill —
the Qt default the style's polish removes — in the constructor, in
`showEvent` (deterministically after polish), and on `StyleChange`
(re-polish), guarded so it is a no-op under styles that leave the default
alone. Portable evidence at the corrected state: clean build, format gate
green, the same composition-aware set 8/8. Open cosmetic question for the
target-native rerun, flagged by the lane: whether restoring the fill costs
the style's rounded-panel rendering at the viewport corners — the native
visual-acceptance pass owns judging that, not this check.

## The corrected fix is proven target-native — 2026-08-28, night

At `4fdab287` in the `windows-lane` worktree (clean fast-forward, fresh
image launch-probed allowed):

- Three-way course, all PASS: `windows11` **12.73 s** — the first
  default-style pass this card has ever had — no-override 13.04 s,
  `windowsvista` 13.13 s. The Qt sweep passed 6/6 in 46.82 s.
- Raster confirmation that the diagnosis, not just the check, is answered:
  in the same viewport region the diagnosis measured, `windows11` went
  from Base 0.0% / `#f6f7f9` 70.2% to **Base 52.40% / `#f6f7f9` 6.41%**,
  within ~0.7 points of the `windowsvista` reference (53.07% / 6.68%).
  The inversion is fully undone, not nudged past the assertion.
- The corner question is answered in two parts that disagree, and the
  distinction is recorded: in an isolated bare-widget probe the restored
  fill overpaints ~5 antialiased arc pixels per corner (outer border
  pixels intact) — real but sub-pixel-scale; in the actual product frame
  it does not manifest at all, because the panes sit flush inside
  splitters behind the gutter and divider, and the pane boundary is a
  hard square edge under `windowsvista` too. Caveat kept: this is the
  1.0x offscreen frame; a real-DPI native run could expose panel edges
  this layout hides, and cosmetics remain the native visual-acceptance
  pass's call. The lane also recorded catching and correcting a
  symbol-collision defect in its own zoom tooling before reading the
  corner pixels, so the corner figures come from the corrected tool.

What item 3 still owes, by this card's own opening measurement: the
default-style course on the **native `windows` QPA at real display
scaling** on the designated host — the offscreen runs were the debuggable
repro, and the original failure was measured native at 125/150/200%.
