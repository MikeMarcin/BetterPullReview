---
schema: "implement-rfc.card.v1"
id: "BPR-0628"
title: "Yield between bounded batches of native pane measurement"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0627"]
acceptance: [{"criterion":"Native geometry equivalence","outcome":"pass","revision":"49aefc1cf2c196168c280d0d7d3d9176251a72e9","evidence":{"kind":"observation","detail":"Native wrapped Unicode/mixed-height reference equality and invalidation pass at 1x/1.25x/2x."}},{"criterion":"Bounded continuation and cancellation","outcome":"pass","revision":"49aefc1cf2c196168c280d0d7d3d9176251a72e9","evidence":{"kind":"observation","detail":"Real input between batches, pending line reveal, font restart, replacement, clear and destruction pass in the selected workbench; no partial axis, stale frame, or repeat idle measurement."}},{"criterion":"Composed large review","outcome":"pass","revision":"49aefc1cf2c196168c280d0d7d3d9176251a72e9","evidence":{"kind":"observation","detail":"Final local six-lane run passes 6/6 in 31.54s, including 25.41s large application course and inspected full-window raster; 94 batches/93 input callbacks, maximum 496 lines and 5.356ms input gap, exact source/navigation/scroll/final identity."}},{"criterion":"Timing limits and remaining work","outcome":"pass","revision":"49aefc1cf2c196168c280d0d7d3d9176251a72e9","evidence":{"kind":"observation","detail":"4ms target checked between groups with 512-line cap; observed native input p95 4.488ms. Whole initial publication maximum 297.119ms remains tracked in BPR-0629, with no universal or RFC-wide performance claim."}}]
---

# Yield between bounded batches of native pane measurement

BPR-0627's temporary phase instrumentation of the real 20,000-line course on
source `d698b338` measured 164.10 ms for the left pane and 163.25 ms for the
right pane in one GUI callback. Axis construction/adoption were 1.20/0.43 ms.
Native block measurement accounts for the remaining approximately 339 ms pause.
Keep the actual Qt geometry; do not substitute uniform line-height estimates.

## Bounded acceptance and ownership

- Own native measurement under `src/adapters/qt/{include/bpr/qt,src}/measured_alignment_axis.*`,
  its `diff_workbench` integration, narrow `comparison_scroll_controller`
  suspension/resume support, and focused Qt/composed tests. Retain a
  bounded amount of GUI-thread measurement per continuation, with input/event
  processing opportunities between batches. Qt widgets remain on the GUI thread.
- Publish only a complete, validated two-pane axis for the exact current
  comparison and presentation generation. Replacement, clear, shutdown, font,
  zoom, width/wrapping, DPI and layout invalidation must cancel/restart stale
  measurement without stale callbacks installing geometry.
- Keep viewport/line boundary values byte-for-byte or numerically equivalent to
  the retained synchronous measurement for native mixed-height/wrapped/Unicode
  cases. Preserve cached scrolling, exact endpoints, selection and source-line
  identity; no partial axis, source truncation or hidden budget increase.
- Prove bounded batches and a real input callback between them, plus cancellation
  and invalidation during measurement. Exercise the actual stored large-review
  comparison and its exact rendered source/scroll/navigation course on this Mac.
- Target short measurement turns (initial design budget 4 ms, checked between
  bounded line groups) and record actual maximum/p95 intervals. One indivisible
  Qt block layout is a stated limit, not an assumed universal time bound.
  BPR-0629 owns the separate approximately 111 ms semantic-index step.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
No new performance subsystem or production logging surface. All tests stay local
with synthetic content and denied network access. No GitHub Actions, push, paid
CI, credentials or production app launch. Native Windows/Linux budgets and
pathological individual-line policy remain separate RFC-0013 acceptance.


## Implementation and acceptance

Helper `52a33b68` and integration `49aefc1cf2c196168c280d0d7d3d9176251a72e9` retain native Qt block
measurement in a resumable state. Each continuation advances at most 512 total
lines in groups of at most 16, checking a 4 ms target between groups. Completed
panes are validated once; checking their continued validity does not rescan
boundaries. Measurement remains entirely on the GUI thread.

The exact immutable result and measurement-state identity bind each generation.
Observable document/layout identity, revision, font, viewport, DPR, wrapping,
tab width and margin changes reject a partial measurement. Workbench events
cancel/restart it; clear/replacement/destruction discard queued work. Only two
complete native measurements can construct and adopt an axis. Shared scrolling
is suspended while geometry is stale, retaining its same-comparison semantic
anchor; pending semantic selection is revealed when the complete axis arrives.

Direct evidence on this revision:

- `ctest --test-dir build/dev --output-on-failure -R '^(qt_diff_axis_scale_(100|125|200)|qt_diff_workbench_scale_100|qt_workbench_window_scale_100|application_large_review_benchmark)$'`
  passes 6/6 in 31.54s. Axis tests compare exact native boundaries/extents with
  the retained synchronous implementation for wrapped Unicode and mixed-height
  blocks, and reject changed/destroyed sources. Workbench tests deliver real
  wheel and semantic-selection input between batches, invalidate font during
  measurement, replace/clear pending comparisons, and destroy a pending widget.
  Existing endpoint, cached-scroll, selection, DPI and layout checks pass.
- The large composed course passes in 25.41s: 515 files, 1,000 comments/threads,
  exact 20,000-line panes and 20 changes, 29 fixture requests, 32 real file
  choices (31 with diff work outstanding), and 1,000 idle wheel events produce
  exactly 1,000 shared frames with no index/geometry rebuild or pane measurement.
  Its inspected 1600x1000 full-window raster shows the exact selected fixture,
  native source panes, diff colors and line numbers without source truncation.
- Native measurement takes 94 continuations, maximum 496 lines in this run;
  93 input callbacks run while it is pending. Consecutive input gaps after
  batching starts are maximum 5.356 ms / p95 4.488 ms (1 ms timer). This excludes
  the initial publication delay, separately measured by the phase timer.
- The large-selection phase is 2,329.20 ms total, maximum/p95 event-loop interval
  297.119/8.205 ms. Rapid selection is 9,239.41 ms, maximum/p95 50.437/6.071 ms;
  the conversation tree remains at most two relevant roots. Load is 11,687.45 ms
  (max 39.951 ms), idle wheel dispatch 383.09 ms, supersession drain 1,072.44 ms.
  The final cache remains two results / 3,517,512 bytes.
- The narrow controller suspension/anchor check, workbench courses at 1.25x/2x,
  and unavailable/binary/supported-recovery course passed in the integration
  run before the output-equivalent completed-pane early return. Their inputs
  remain relevant; no unrelated suite was repeated. The older window scale
  test initially assumed synchronous completion; its wait now observes pending
  geometry before taking rendering/scroll evidence and the final lane passes.
- Documentation structure and `git diff --check` pass. All commands run locally
  on the Mac with network-denied synthetic CTest fixtures. No live account or
  production process participates.

Machine context: M4 Pro, 48 GiB, 14 logical processors, macOS 26.6.2 arm64,
Debug Qt 6.10.3, offscreen/Fusion, DPR 1 for the composed course. The 4 ms target
is not an absolute bound on an indivisible native block/group, nor a portable
latency guarantee. BPR-0629 still owns semantic-index preparation; its scope
also needs to account for the host's repeated decision-lane validation found
while inspecting the remaining publication stall. Text installation and all
other RFC-0013 closeout criteria remain open.
