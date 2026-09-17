---
schema: "implement-rfc.card.v1"
id: "BPR-0859"
title: "The UIforETW selected-diff course finds intraline highlights no longer painted as their roles"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0859: The UIforETW selected-diff course finds intraline highlights no longer painted as their roles

## Defect

`application_uiforetw_selected_diff_contract` fails on Linux at `a1864fa2`
and on macOS at `20d99914`. The output is `UIforETW light capture failed (1)`,
`PUBLIC_PATH_FAILED` and `presentation=0`.

Startup completes: the output shows `Pull request ready.`,
`GITHUB_FIXTURE_OK:uiforetw-74` and the first three `UIFORETW_*_OK`
markers. The first failure is `UIFORETW_RESOURCE_ROLE_FAILED`:

- `ResourceRolePaint` is set in `src/app/src/github_application.cpp` ~16340
  by `intraline_roles_visible` (~15386).
- That check counts intraline highlights on each pane whose colour matches the
  appearance profile, or which are underlined.
- After 64 waits it fails.

Every later marker fails only because the probe stops there.

The diff lands on the right blobs and finds the intraline pair, so what
changed is the painting, or the check's expectation of it. The scout suspects
`a5427499` (2026-09-15, "Rebuild the Appearance page around the theme on
screen"), which moved the pane's highlight painting (`append_line_band`,
`character_offset`) into `code_sample_painter`. `867e4bed` (one saved
palette per theme) is the second candidate. The scout's confidence is
medium-low. The row passed when BPR-0723 closed on 2026-09-11.

## Scope

- Confirm the introducing commit, for example by running the row just before
  `a5427499`.
- Name whether the product's intraline role painting regressed (a visible
  defect) or the probe's colour/underline expectation is stale. Fix that side.
- A `src/app` change reseals the round-trip pins with a recorded review.

## Acceptance

- The row passes on macOS and Linux.
- The intraline roles are painted as the appearance profile defines them, and
  that is shown in the rendered application, not only in a model test.

## Second scout (dsp-004, read-only): suspects narrowed

A code-reading trace did not confirm the suspected commits:

- `a5427499` moved `character_offset`, `append_line_band` and
  `append_selection_band` into `code_sample_painter.cpp` with identical
  bodies. `append_intraline_emphasis` (`diff_workbench.cpp` ~2505-2624) and
  its colour sources are unchanged.
- `867e4bed` does not touch the diff workbench or the probe.

The probe reads the panes' own `extraSelections()`, and it accepts the role
colour, the colour under the selection wash, and the derived fill. It also
accepts an underline, although the code at HEAD sets none.

The remaining candidates are runtime conditions, so bisect from the last
green run rather than assuming a commit:

- (a) the target line is outside `paintable_line_bounds` when the probe
  samples;
- (b) the intraline pairs do not match the pane line;
- (c) `QColor ==` compares the colour spec, so the derived-fill branch can
  mismatch;
- (d) a timing change elsewhere.

Log each pane's highlight colours and specs, the accepted colours and the
paint bounds at the failure. That separates a product repaint defect from a
stale probe.

## Fix, review and reseal (dsp-005, Mac)

**The role-paint check was never reached.** `reveal_intraline_pair` refused
first and printed nothing, because the window was on Overview, which composes
no comparison: the panes held the diff's *text*, which is why every earlier
marker passed, but the surface carrying them was hidden, so the geometry axis
was never measured and the selected change was never scrolled into view. The
decisive line (`build/r2-logs/dsp5-0859-instr3.log`):

```
UIFORETW_REVEAL_PAIR_OFFSCREEN:left-first line=96 placed=1 rect=283,-90 9x15
 viewport=565x675 vscroll=102/102 top_block=102 shown_pane=0
```

`shown_pane=0` with the pane pinned at the document's end and unmoved after
256 further ticks, so not settling, and therefore not candidate (d).

**A stale probe, not a product defect — the fourth course in this run stranded
on Overview** ([[BPR-0823]], [[BPR-0858]], [[BPR-0860]]; [[BPR-0863]]). No
product colour or paint change was needed. **Candidate (c) is positively ruled
out** rather than merely unobserved: profile colours are built by
`to_presentation_color` as `QColor{int,int,int}`, i.e. Rgb spec, the workbench
copies exactly those, and both the product's `derived_emphasis_fill` and the
probe's `role_renderings` build the derived fill the same way from the same
profile. No spec mismatch was possible on the shipped path.

**Two further stale expectations were hiding behind the first.**

- The drawer toggles were set only inside `if (action->isChecked())`, so they
  asserted the old opening modality rather than the toggle. Each now presses
  twice and proves its wire whatever the drawer starts as.
- The capture asserted an exact 24-action strip; `ce62ed52` added Home's
  Group, State headers and Filter, so the expectation is the row the product
  now composes, still exact, ordered and whole.

**The card's premise was also wrong** about the 64 waits: a `probe.Waits = 0U`
ran on every tick of the phase, so that cap was unreachable dead code. Had the
role check ever been the failing one, the course would have burned the whole
40 s budget on a stage number.

**Criterion 2 is met in the rendered application.** The capture stage, which
had not run since the regression, now completes for all four appearance
profiles. It measures the rasterised viewport, not the model: `role_pixel_count`
grabs `viewport()->grab(rect).toImage()` and counts pixels whose exact ARGB is
the profile's role colour, that colour under the selection wash, or the derived
fill, over the span's own rectangle. Evidence under
`build/r2-dev/tests/end_to_end/uiforetw-selected-diff-contract/evidence/<profile>/`:

| profile | removed role | added role | exact row |
|---|---|---|---|
| light | `#fff1b9b4` | `#ff9effb5` | true |
| dark | `#ff933030` | `#ff145e20` | true |
| high-contrast-light | `#ffff9999` | `#ff55ff55` | true |
| high-contrast-dark | `#ffff9999` | `#ff55ff55` | true |

Five intraline edits per profile with `fixture_diff_helper_invocations: 0`, so
the shipped drawer-selection wire rather than the test shortcut.

**Six named terms** now print what they saw instead of that they did not see
what they wanted, including each mark's background colour **and its
`QColor::spec()`** — which would have made candidate (c) decidable in one
line. They are silent on a passing run.

**Independent review: no blocker.** An `rfc-reviewer` was asked specifically to
falsify the two assertion *changes*. It quoted both before and after and found
the toggles **equal on the wire and stronger on coverage** (the only thing the
old form proved and the new does not is the incidental precondition that all
three drawers start expanded, which is the pre-BPR-0661 opening state and not
an acceptance criterion; the focused-layout stage still asserts the same
terminal state), and the strip row **equal in form with three more conjuncts**,
still an exact ordered whole-row conjunction with the same pointer-identity
comparisons. It walked every other hunk and found nothing deleted, made
conditional or operator-loosened, confirmed the dead-cap pattern is gone from
the whole probe, and confirmed the capture cannot be skipped by the new phase.

**The review also caught a cross-lane break neither station could see.** The
exact 27-action row would have been **false on Windows** once [[BPR-0653]]
landed, because that card inserts `workbenchStripMenuAction` at index 1 —
shifting every later action by one — and ends the row with
`workbenchCaptionButtonsAction`, making 29. Both cards were merged in the same
pass, so the expectation is now platform-aware through a lead and a trailing
offset rather than a second copy of the row: it stays exact, ordered and whole
on every platform, and a control added to one platform cannot be absorbed by
the other's expectation. This is why the row is worth watching on the Windows
station even though this card's acceptance names only macOS and Linux.

**Runs (macOS).** The row passes 3/3 at 68.5, 67.6 and 67.8 s, plus 68.1 s
again after the platform-aware row and once more at the resealed head. It went
from 11 s to ~68 s because the course now actually completes all four profiles
including the capture: about 17 s per profile process against a 40 s budget.
`src/app` was touched, so the pins are resealed in the merge commit on this
review and BPR-0653's.

**Still open.** Criterion 1 names macOS **and Linux**. The Linux run at the
fix is owed: the "18 of 18" Linux pass was at `754d0538`, which predates the
fix. The card stays In Progress for it.
