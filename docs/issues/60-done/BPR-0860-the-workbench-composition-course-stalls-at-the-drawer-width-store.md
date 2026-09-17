---
schema: "implement-rfc.card.v1"
id: "BPR-0860"
title: "The workbench composition course stalls at the drawer width store until its run budget expires"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
acceptance: [{"criterion":"The row passes on macOS and Linux","outcome":"pass","revision":"2b2e5549","evidence":{"kind":"command","detail":"application_workbench_composition_contract passes on macOS 6/6 at 21.4, 21.3, 21.3, 21.7, 21.8 and 21.9 s at load averages 2.2-4.5, and at 21.27 s in the full macOS sweep at 2b2e5549; and on Linux at 2.80 s in bpr-linux's merged-head runs and 4.07 s in its full sweep at the same commit."}},{"criterion":"A stalled width store fails with a named term, not by running out the budget","outcome":"pass","revision":"2b2e5549","evidence":{"kind":"observation","detail":"Both width phases are capped at 512 ticks (about 5.3 s at the run's 10 ms tick) and name their stall. Three deliberately broken builds, each reverted, showed each term firing: a forced-zero drag delta gives WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED, an early return before the host resize gives WORKBENCH_DRAWER_WIDTH_STORE_STALLED with dispatches=1, and triggering overview instead of review gives WORKBENCH_FILES_DRAWER_EXPAND_STALLED -- all three exiting at about 16.6 s against 40.38 s and a bare RUN_BUDGET_EXPIRED before. Each line prints the dispatch and store counters with their expectations, outstanding requests, splitter sizes, both drawer states, modality, drawer controls and profile scoping, which is what let the Linux run confirm the structural assumption in one pass. An independent rfc-reviewer confirmed the cap is bounded well inside the budget and that no original assertion was loosened."}}]
---

# BPR-0860: The workbench composition course stalls at the drawer width store until its run budget expires

## Defect

`application_workbench_composition_contract` fails on both platforms:

- on Linux at `a1864fa2`, with `RUN_BUDGET_EXPIRED at 40028 of 40000 ms`;
- on macOS at `20d99914`, with `RUN_BUDGET_EXPIRED … elapsed_ms=40020 stage=2`,
  then `WORKBENCH_DRAWER_WIDTH_WIRE_FAILED` and the later markers.

The probe drags `central_splitter()->move_handle(2, …)`
(`src/app/src/github_application.cpp` ~14881). It then waits in
`awaiting_width_store` (~14889-14906), which has no cap on its waits. Neither
the width dispatch nor the persisted layout arrives, so the course runs until
its budget ends. The later markers fail only because the probe never reaches
them.

[[BPR-0718]] fixed an earlier failure of this row. The scout named candidate
commits since 2026-09-11, each with low confidence:

- `ad0bb67d`: Dif in the AI tools area, in `workbench_window.cpp`;
- `3dcbf237`: BPR-0729's one-pane comparison, which changed the layout store;
- `ce62ed52`;
- `ed2b3f14`.

## Scope

- Find the introducing commit.
- Name whether handle 2 is still the files-drawer edge, and whether a drag
  still dispatches and persists the width.
- Fix the product or the probe at the cause.
- Cap the phase's waits so a stall fails by name before the run budget ends.
- A `src/app` change reseals the round-trip pins with a recorded review.

## Acceptance

- The row passes on macOS and Linux.
- A stalled width store fails with a named term, not by running out the
  budget.

## Second scout (dsp-004, read-only): what code reading rules out

**Ruled out.** The splitter order at HEAD is repositories, files, centre,
conversations (`workbench_window.cpp` ~1418), so handle 2 is still the
files-drawer edge. Every link from drag to store is intact:

- `move_handle`
- `splitterMoved`
- `observe_handle_drag` (~7844), which requires a published layout, an
  expanded files drawer and 4 widgets
- `observe_drawer_width`
- `dispatch_workbench_width` (`github_application.cpp` ~7394)
- `resize_now` (`workbench_application_host.cpp` ~1020)
- `submit_settings_store` (~770)
- `finish_settings_store` (~1455), or, on profile defaults, `accept_preferences` (~1167)

`ad0bb67d`, `3dcbf237` and `ed2b3f14` break no link.

**Remaining candidates**, all runtime conditions:

- (a) the files drawer cannot grow in the offscreen window, so the size is
  unchanged and nothing is dispatched;
- (b) the course is on the profile-defaults branch, and `ce62ed52`'s
  empty-Home defaults make `accept_preferences` mismatch, so the stored count
  never moves (a product regression);
- (c) no layout was ever published (`LayoutWidths` is empty).

A user dragging the drawer edge still persists the width, by the code, unless
(b) applies.

**Next step.** Cap the wait at ~14887-14896 and fail with separate terms for
"no width dispatch" and "no stored layout". Emit the splitter sizes, the
drawer state, `LayoutWidths` and whether the profile is scoped. Bisect from
the last green run.

## Fix, review and reseal (dsp-005, Mac)

**Cause: a stale probe, and none of the scout's three candidates.** The course
opens its pull request from the Home bar ([[BPR-0771]]), so the window opens
on [[BPR-0661]]'s default `OpenModality`, Overview. Since [[BPR-0727]]
(`7885e2d2`, 2026-09-11 — the last green day) the core collapses all three
drawers inside `set_workbench_modality_intent`, and
`review_behaviour_settings::DrawerControls` defaults to `hidden`, so a
collapsed drawer takes **zero** width and hides itself. `observe_handle_drag`
correctly refuses to report a collapsed drawer's width, so dragging handle 2
dispatched nothing at all.

The first instrumented failure named it in one line
(`build/r2-logs/dsp5-0860-instr2.log`):

```
WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED:waits=513 dispatches=0 expected_dispatches=1
 persisted_layouts=1 layout_updates=1 requested_files_width=361 initial_files_width=320
 host_files_width=320 splitter_widgets=4 splitter_sizes=0,0,1280,0
 files_drawer_state=collapsed host_files_drawer=collapsed modality=Overview
 drawer_controls=hidden showing_overview=true persistence_available=false profile_scoped=false
```

That line also **rules out the scout's other two candidates**: `layout_updates=1`
means a layout was published, so not (c); `profile_scoped=false` means the
course is not on the profile-defaults branch, so not (b), and `ce62ed52` is
exonerated.

**Scope answers.** Handle 2 is still the files-drawer edge — the splitter
order is repositories, files, centre, conversations, and `sizes()` still
reports four widgets even when drawers are hidden. A drag still dispatches
and persists the width; negative run B below is the positive proof.

**No product defect.** The collapse is a deliberate owner-driven decision
([[BPR-0674]], [[BPR-0724]], [[BPR-0727]]): a reviewer in Overview has no
files-drawer edge to drag, leaves Overview first, and then the drag dispatches
and persists. A width set in Review is retained in `Layout.FilesWidth` and
re-granted on re-expansion. The course was asserting a gesture in a state
where the product intentionally offers none.

**The change**, one file, `src/app/src/github_application.cpp` (+145 / −7), all
inside the workbench-composition probe:

- A new `awaiting_files_drawer` phase enters **Review** through the strip's
  public modality action. Review is the only modality whose files drawer is
  expanded (`workbench_window.cpp` ~4224: collapsed for Overview, Read and
  Discuss), so [[BPR-0858]]'s Read entry would not have worked here.
- It waits until the window and host agree the drawer is expanded, the
  splitter has four widgets with a non-zero files size, and
  `outstanding_requests() == 0`, so the modality's own store lands before the
  drag's baselines are taken and the next store is attributable to the drag.
- The drag now moves the handle **by the reviewer's delta**,
  `move_handle(2, handle(2)->x() + (target - sizes[1]))`, instead of the
  absolute `sizes[0] + handleWidth() + target`, which assumed a visible
  repositories drawer. The handle width cancels in both the collapsed and
  expanded cases; the old form was itself off by `handleWidth()` when
  repositories was hidden.
- Both width phases are capped at `workbench_width_store_waits = 512`
  (~5.3 s at the run's 10 ms tick) and name their stall.
- **Every original assertion on the width wire is unchanged**, including the
  strict `== base + 1` counts.

**Criterion 2's named terms** are `WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED`,
`WORKBENCH_DRAWER_WIDTH_STORE_STALLED` and
`WORKBENCH_FILES_DRAWER_EXPAND_STALLED`. Each was shown to fire by a
deliberately broken build, all three reverted:

| Break | Log | Result |
|---|---|---|
| drag delta forced to 0 | `dsp5-0860-negA.log` | `…DISPATCH_STALLED`, exit 1 at 16.53 s |
| `dispatch_workbench_width` returns before the host resize | `dsp5-0860-negB.log` | `…STORE_STALLED` with `dispatches=1`, exit 1 at 16.72 s |
| strip triggers `overview` instead of `review` | `dsp5-0860-negC.log` | `…EXPAND_STALLED`, exit 1 at 16.62 s |

All three name themselves at about 16.6 s, against 40.38 s and a bare
`RUN_BUDGET_EXPIRED` before (`dsp5-0860-repro1.log`).

**Independent review: no blocker.** An `rfc-reviewer` checked the eight risks
this change carried and confirmed at file and line that no assertion was
loosened, that `outstanding_requests()==0` cannot let the modality's own store
satisfy the drag's assertion, that Review really is the only modality with an
expanded files drawer, that the delta arithmetic is correct with repositories
collapsed or expanded and under any handle width, that no negative-experiment
residue survives, and that panels are never removed from the splitter, so
`sizes().size() == 4` holds under the Linux `QT_STYLE_OVERRIDE=Windows` path.
Its two non-blocking findings were adjudicated:

- **Taken.** The stall line printed `waits=513` against a cap of 512, which
  read as if the cap were 513. It now also prints `wait_cap=`.
- **Recorded, not taken.** The `outstanding_requests()==0` guard does not
  cover the profile-defaults store branch, where the store completes in
  `accept_preferences` without an `Outstanding` record. On that branch the
  modality's store could complete after the baseline and produce a spurious
  `…STORE_STALLED`. It is unreachable in this row, which runs
  `profile_scoped=false`, and the stall line prints `profile_scoped=`, so an
  occurrence would be diagnosable in one run rather than a bisect. Widening
  the guard was not part of this card's cause.
- The reviewer also corrected an attribution: the Read and Discuss
  files-collapse is [[BPR-0674]]'s rule, not [[BPR-0724]]'s.

**Reseal.** `src/app/src/github_application.cpp` is pinned, so the full
application and production manifest digests were resealed in this card's
commit on the review above. Nothing committed since the last reseal at
`fe5b4b64` had touched `src/` at all, so the reviewed range is exactly this
change. The mutation-branch pin still has never moved.

**Runs (macOS).** The row passes 6/6: 21.42, 21.25, 21.27, 21.74, 21.81
(`dsp5-0860-ctest{1,2,3}.log`, `dsp5-0860-final2.log`) and 21.88 s after the
`wait_cap` addition, at load averages 2.2 to 4.5 — far below the ~15
spurious-failure threshold. `application_review_round_trip_composition_contract`
(resealed), `test_network_isolation_manifest`, `architecture_boundaries`, the
documentation gate and `git diff --check` all pass
(`dsp5-0860-int{1,2}.log`).

**Follow-ups carded.** [[BPR-0863]] (three courses now stranded on Overview;
give a course a way to state the modality it needs) and [[BPR-0864]] (the same
probe's remaining phases are still uncapped). [[BPR-0862]] records a separate
uncarded red found on the way: the repo-wide formatting gate.

**Still open.** Criterion 1 names macOS **and Linux**, so this card stays In
Progress for the Linux run. The row is also registered on Windows
(`WINDOWS_UNCONTAINED`), so the Windows run is worth taking in the same pass.

## Linux run at the merged head (dsp-005, bpr-linux/int-12)

Station `bpr-linux` (JaegAW, WSL2), branch `linux-lane`, at
`754d05387286b201d2afb13757cd894bccfddcbd` — the merged head, equal to `main`
— on a clean tree, after a stock `cmake --preset dev --fresh` configure and a
`--clean-first` build that was warning-clean under GCC 15.2.0. Its own serial
run, `ctest --test-dir build/dev -j1 -V -R '^application_workbench_composition_contract$'`,
from 18:52:01Z, at a 1-minute load average of 9.8 — below the ~15 threshold
this card records. The Secret Service `login` collection read `(<false>,)`
before the group.

| Row | Result | Time | Marker |
|---|---|---|---|
| `application_workbench_composition_contract` | Pass | 2.80 s | `WORKBENCH_COMPOSITION_CONTRACT_OK` |

One run, as asked; no bisect was needed. Against the 40.38 s
`RUN_BUDGET_EXPIRED` this card opened with, the row now finishes in 2.80 s,
well inside its 120 s timeout and its 40 s run budget.

**`splitter_widgets` is 4 under `QT_STYLE_OVERRIDE=Windows`.** The row is
offscreen *and* Windows-styled by registration, not by an exported variable:
`bpr_public_path_fixture_environment` appends `QT_STYLE_OVERRIDE=Windows` on
`UNIX AND NOT APPLE` (`tests/end_to_end/CMakeLists.txt` ~1086-1092), and the
log's "Environment variables" block shows both `QT_QPA_PLATFORM=offscreen` and
`QT_STYLE_OVERRIDE=Windows`. For this probe a pass measures the widget count
rather than merely being consistent with it: `awaiting_files_drawer` will not
take its baselines or drag until `sizes.size() == 4 && sizes[1] > 0`, with the
window and the host both reporting the files drawer expanded and
`outstanding_requests() == 0` (`src/app/src/github_application.cpp`
~14986-14996). Anything else re-waits and then stalls by name. The probe went
on to satisfy every original width assertion, including the strict
`== base + 1` dispatch and persisted-layout counts, so the splitter reported
exactly four widgets with a non-zero files width. The reviewer's expectation
that a hidden drawer still occupies a slot at size 0 holds on the Linux
Windows-style path.

**No stall term fired.** None of `WORKBENCH_FILES_DRAWER_EXPAND_STALLED`,
`WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED` or
`WORKBENCH_DRAWER_WIDTH_STORE_STALLED` appears in the log, and neither does
`RUN_BUDGET_EXPIRED`. So there is no whole line to quote: the Review entry
through the strip's public modality action and the drag by the reviewer's
delta both work on Linux, and the capped waits were never reached.

A passing run prints none of the probe's own marker lines, because the
contract's CMake script captures the probe output into `probe_output` and
prints it only when the process fails
(`tests/end_to_end/application_workbench_composition_contract.cmake` ~74-100).
The contract marker above is what a green row emits.

Criterion 1 named macOS **and Linux**; both now pass. Windows
(`WINDOWS_UNCONTAINED`) is the only run this card still owes. The run record
is in `docs/verification/dsp4-linux-confirmation.md`.

## Linux confirmation, and the card closes (dsp-005, integrator)

`bpr-linux` passes this row at the merged head and again in its full sweep at
`2b2e5549` (2.80 s and 4.07 s), and it confirms the one structural assumption
the fix rests on. The pass **measures** it rather than merely being consistent
with it: `awaiting_files_drawer` refuses to take baselines or drag until the
splitter reports four widgets with a non-zero files size and both the window
and the host call the drawer expanded, so a hidden drawer does still occupy a
slot at size 0 under `QT_STYLE_OVERRIDE=Windows`. Every original width
assertion then held, including the strict `== base + 1` counts. No stall term
fired and the run budget was not touched.

macOS passes at 21.27 s in the full macOS sweep at the same commit. Both
acceptance criteria are met on both named platforms, so the card is Done. The
Windows run is dispatched with `bpr-win` but is not part of this acceptance.
