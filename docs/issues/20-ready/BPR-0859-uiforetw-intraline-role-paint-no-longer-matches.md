---
schema: "implement-rfc.card.v1"
id: "BPR-0859"
title: "The UIforETW selected-diff course finds intraline highlights no longer painted as their roles"
status: "Ready"
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
