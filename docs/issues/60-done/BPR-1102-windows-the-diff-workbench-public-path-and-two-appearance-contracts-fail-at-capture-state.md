---
schema: "implement-rfc.card.v1"
id: "BPR-1102"
title: "Windows: application_diff_workbench_public_path and the two appearance contracts (theme_customization, store_fallback) fail at DIFF_INTERACTION_FAILURE:capture-state"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's application sweep","found_at":"bcd57840","first_seen":"b9991925 (BPR-1022's rows)","cross_platform_at":"5660cf3b","related_cards":["BPR-1033","BPR-1073","BPR-1050","BPR-0265"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
acceptance: [{"criterion":"AC1 A failing run names the capture step and which of its four clauses is false, with the facts that clause depends on, shown in a real Windows run","outcome":"pass","revision":"4248720b","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial ctest: application_diff_workbench_public_path printed DIFF_INTERACTION_CAPTURE_STATE_FACTS: step=01-selected-composition retained_frame=1 link_map_frame=0 context=1 selection=1 geometry_pending=1 geometry_rebuilds=1 measurement_batches=21 measurement_restarts=1 max_batch_lines=32 link_map=visible:1,size:56x857,links:0, after the unchanged DIFF_INTERACTION_FAILURE:capture-state marker.","path":"docs/issues/60-done/BPR-1102-windows-the-diff-workbench-public-path-and-two-appearance-contracts-fail-at-capture-state.md#named-fixed-in-the-capture-and-the-rows-pass-on-windows-bpr-main-win-2026-09-24"}},{"criterion":"AC2 The cause is named and fixed in the layer that owns it, and the three rows pass on Windows in N of N runs (N >= 3 each)","outcome":"pass","revision":"4248720b","evidence":{"kind":"command","detail":"Cause: a window resize after the first measurement restarts the panes' time-sliced measurement (49aefc1c) and clears the link map's geometry; on the Windows Debug build (Qt debug libraries) a 4 ms batch measures at most 32 lines, so the capture's fixed four passes end before the frame exists (reverted trace: measurement complete at t+1286 ms, restarted at t+1386..1463 ms, capture at t+2007 ms still pending). Fix in the BPR_BUILD_TESTING capture: wait bounded at 5 s for geometry_pending() to clear, then the four passes. Serial runs: diff workbench 4 of 4 (22.05-22.35 s), theme customization 4 of 4 (25.97-27.36 s), store fallback 4 of 4 (23.24-23.50 s); round-trip composition contract passes (6.66 s) with the manifest pin resealed; clang-format clean; clang-tidy passes on main.cpp.","path":"docs/issues/60-done/BPR-1102-windows-the-diff-workbench-public-path-and-two-appearance-contracts-fail-at-capture-state.md#named-fixed-in-the-capture-and-the-rows-pass-on-windows-bpr-main-win-2026-09-24"}},{"criterion":"AC3 Negative control: with the fix disabled, the rows fail again on Windows at the clause AC1 names","outcome":"pass","revision":"4248720b","evidence":{"kind":"command","detail":"Same build with the wait's bound set to zero (only the four passes): application_diff_workbench_public_path fails at DIFF_INTERACTION_FAILURE:capture-state in 8.87 s with step=01-selected-composition link_map_frame=0 geometry_pending=1 max_batch_lines=32.","path":"docs/issues/60-done/BPR-1102-windows-the-diff-workbench-public-path-and-two-appearance-contracts-fail-at-capture-state.md#named-fixed-in-the-capture-and-the-rows-pass-on-windows-bpr-main-win-2026-09-24"}},{"criterion":"AC4 macOS and Linux keep their path, or the change is run there","outcome":"pass","revision":"4248720b","evidence":{"kind":"freeform","detail":"The wait's condition is false on entry wherever the measurement has completed, so no extra event pass runs, and the facts line prints only on failure; the change is inside BPR_BUILD_TESTING in capture_diff_interaction_evidence. The integrator's 5660cf3b runs (macOS 12.56/24.37/12.81 s, Linux 2.78/5.56/3.29 s, all pass) are the baseline; no macOS or Linux run was made from this station."}}]
---

# BPR-1102: Windows: application_diff_workbench_public_path and the two appearance contracts (theme_customization, store_fallback) fail at DIFF_INTERACTION_FAILURE:capture-state

## Defect

Three rows fail on Windows, and only on Windows, at the same line:

- `application_diff_workbench_public_path`
- `application_appearance_theme_customization_contract`
- `application_appearance_store_fallback_contract`

```text
DIFF_INTERACTION_FAILURE:capture-state
...
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:23
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
```

The line comes from the diff interaction evidence course in `src/app/main.cpp`
(`capture_diff_interaction_evidence`), which all three rows run. Each capture
settles a fixed four event passes and then requires four things together: the
scroll controller's retained frame, the link map's rendered frame, the
workbench's current context and its selection. The failure line does not say
which of the four is missing, or at which of the nineteen captures.

## Measured

| Station | Revision | Result |
| --- | --- | --- |
| bpr-main-win (MSVC 19.51 Debug, Qt 6.11.2, offscreen, `QFusionStyle`) | `b9991925` | diff workbench row fails at `capture-state` ([[BPR-1022]]'s rows, 11.24 s) |
| bpr-main-win, `build/windows-dev` | `bcd57840` | all three fail at `capture-state`, 4 of 4 runs each: 8.97 to 9.09 s, 11.95 to 12.08 s, 8.95 to 9.07 s |
| bpr-main-mac (Apple clang, Qt 6.11.2) | `5660cf3b` | all three pass: 12.56 s, 24.37 s, 12.81 s |
| bpr-main-linux (GCC 15.2, offscreen, load 0.30) | `5660cf3b` | all three pass: 2.78 s, 5.56 s, 3.29 s |

The macOS and Linux runs were made by the integrator and relayed to this
station; they print no `DIFF_INTERACTION_FAILURE` or other failure line. The
Windows logs are station records under `build/logs/bpr-1050-main-win/` and are
not committed.

## Scope

- Name the failing clause and the step first, as [[BPR-1033]] did for the
  UIforETW capture: one term per clause, with the facts the missing one waits
  on, printed after the existing marker so every earlier line keeps its place.
- Then fix the cause in the layer that owns it. Look first at what is
  Windows-specific on the capture's path.
- Keep the macOS and Linux behaviour: the rows pass there today.

## Acceptance

- AC1 A failing run names the capture step and which of its four clauses is
  false, with the facts that clause depends on, shown in a real Windows run.
- AC2 The cause is named and fixed in the layer that owns it, and the three
  rows pass on Windows in N of N runs (N >= 3 each).
- AC3 Negative control: with the fix disabled, the rows fail again on Windows
  at the clause AC1 names.
- AC4 macOS and Linux keep their path, or the change is run there.

## Named, fixed in the capture, and the rows pass on Windows (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2 debug
libraries, offscreen, `QFusionStyle`), serial, at `f346ae84` plus this change.
The premise held at `f346ae84`: all three rows failed at `capture-state`
again (9.19 s, 12.15 s, 8.94 s). The logs are station records under
`build/logs/bpr-1102-main-win/` and are not committed.

**The clause is named (AC1).** After `DIFF_INTERACTION_FAILURE:capture-state`
the capture now prints one line with the step, one term per clause, and what
the link map's frame waits on. It prints on failure only and after the marker.
The first Windows run printed:

```text
DIFF_INTERACTION_CAPTURE_STATE_FACTS: step=01-selected-composition
retained_frame=1 link_map_frame=0 context=1 selection=1 geometry_pending=1
geometry_rebuilds=1 geometry_failures=0 measurement_batches=21
measurement_restarts=1 link_map=visible:1,size:56x857,links:0
```

So it is the first capture, and only the link map's frame is missing. The
workbench is still measuring its panes.

**The cause.** Since `49aefc1c` the workbench measures its panes in time
slices: each batch stops after about 4 ms and queues the next with a zero
timer, and the link map gets its geometry, and with it its frame, only when
the measurement completes. A resize restarts the measurement and clears the
link map's geometry first. The capture settles a fixed four event passes, the
count [[BPR-0265]] gave it before the measurement was sliced. A reverted trace
in `schedule_geometry_rebuild` and at the capture showed the order on this
station:

| Time | Event |
| --- | --- |
| t+0 ms | The comparison is published (`accepted_diff`); pane resizes, the show event and workbench resizes follow as the window lays out |
| t+889 ms | It is published again (`accepted_diff`) |
| t+1286 ms | The measurement completes: 111 + 115 lines in 16 batches |
| t+1386 to t+1463 ms | The window grows (workbench 632x697 to 632x877): two pane resizes and a workbench resize restart the measurement |
| t+2007 ms | The first capture starts, with the measurement pending |
| t+2068 ms | After its four passes it is still pending: 21 batches in all, no link map frame |

What is Windows-specific is the size of a batch. The facts' largest batch on
this station is 32 lines (`max_batch_lines=32`), so the fixture's 226 lines
need about 16 batches, and each event pass runs at most one. This Debug build
links Qt's debug libraries (`Qt6Guid.dll`, `Qt6Widgetsd.dll`), which MSVC's
debug runtime requires, and the block layouts the measurement reads are
unoptimized there. On macOS and Linux the same fixture evidently measures
within the four passes, since the rows pass there.

**The fix, in the course.** Each capture now waits, bounded at 5 s, until the
workbench no longer reports its geometry pending, and then drains its four
passes as before. It is the wait [[BPR-1033]]'s capture uses. Where the
measurement is already complete the wait returns at once, so a platform that
passed does exactly what it did before. An expiry prints the facts line above
with `geometry_pending=1`. No product path changes: the whole change is in
`capture_diff_interaction_evidence`, inside `BPR_BUILD_TESTING` in
`src/app/main.cpp`.

**Negative control (AC3).** The same build with the wait's bound set to zero,
so only the four passes remain: `application_diff_workbench_public_path` fails
at `capture-state` in 8.87 s, and the facts line names the same clause
(`step=01-selected-composition ... link_map_frame=0 ... geometry_pending=1 ...
max_batch_lines=32`).

**The rows pass (AC2).** Serial, one `ctest --preset dev -j1 -V -R` call per
run:

| Row | Before (HEAD) | With the fix, 4 runs |
| --- | --- | --- |
| `application_diff_workbench_public_path` | fails, 9.19 s | 4 of 4 pass: 22.35, 22.16, 22.12, 22.05 s |
| `application_appearance_theme_customization_contract` | fails, 12.15 s | 4 of 4 pass: 27.36, 26.53, 25.97, 26.44 s |
| `application_appearance_store_fallback_contract` | fails, 8.94 s | 4 of 4 pass: 23.50, 23.29, 23.24, 23.41 s |

The first run of each was on the fix before formatting; the other three on
the committed content. The longer time is the rest of each course, which the
failure used to cut short. `clang-format` is clean and `clang-tidy` passes on
`main.cpp` with warnings as errors.

**The pins.** `main.cpp` is in the production application manifest that
`application_review_round_trip_composition_contract` pins. The manifest digest
is resealed in the same commit, covering `59915464..` this card's commit;
nothing else touched the pinned scope since BPR-1024's reseal. The
full-application and mutation-branch pins are unchanged. This section is the
review that reseal records. The source-only pin check passes, and the contract
row passes (6.66 s).

**AC4.** The wait's condition is false on entry wherever the measurement has
completed, and the facts line prints only on failure, so macOS and Linux keep
their path. No run has been made there from this station; the integrator's
5660cf3b runs are the baseline.

## Post-close: Linux at `e2dc8f82` (bpr-main-linux, 2026-09-24)

**AC4 now holds on Linux by a run.** The three rows and the round-trip
composition contract pass 4 of 4 with this card's capture wait in the tree.
On macOS AC4 still rests on the reasoning above and the integrator's
`5660cf3b` baseline: this station did not run macOS.

The integrator asked for this rerun. It was made in the station's main
checkout, with a clean tree, at `e2dc8f82` ("Finish BPR-1104"). That revision
contains `4248720b`. The station is WSL2 Ubuntu 26.04 with GCC 15.2 and Qt
6.11.2. The tree is `build/dev` (Debug) from the `dev` preset, which compiles
with `-Werror`. The build of `e2dc8f82` recompiled `src/app/main.cpp` and has
no compiler `warning:` line. The rows ran once each, serially (`-j1`), with
`QT_QPA_PLATFORM=offscreen` and with `DISPLAY` and `WAYLAND_DISPLAY` unset.
They started right after the build, at a load average of 7.97/2.09/0.70.

`ctest --test-dir build/dev -j1 --output-on-failure -R "^(application_diff_workbench_public_path|application_appearance_theme_customization_contract|application_appearance_store_fallback_contract|application_review_round_trip_composition_contract)$"`
passed 4 of 4:

| Row | Before the fix, `5660cf3b` | At `e2dc8f82` |
| --- | --- | --- |
| `application_diff_workbench_public_path` | pass, 2.78 s | pass, 3.10 s |
| `application_appearance_theme_customization_contract` | pass, 5.56 s | pass, 5.48 s |
| `application_appearance_store_fallback_contract` | pass, 3.29 s | pass, 3.32 s |
| `application_review_round_trip_composition_contract` | not run | pass, 1.66 s |

The "before" column is this station's Linux row in the Measured table above,
at `5660cf3b` and before `4248720b` landed. It also passed 3 of 3.

The round-trip contract's pass shows that the production application
manifest digest resealed in `4248720b` matches the Linux checkout after
`main.cpp`'s test-only capture change. The contract hashes the pinned files
with line endings normalized to LF, so a digest sealed on Windows can match
here.

Each row takes within 0.32 s of its time before the fix. That fits AC4's
reasoning, that the wait returns at once where the measurement has already
completed, but this run does not observe the wait directly. With
`--output-on-failure` a passing row prints nothing more. So the log has no
`DIFF_INTERACTION_FAILURE`, no `DIFF_INTERACTION_CAPTURE_STATE_FACTS` and no
other stopping line.

The logs are station records under
`build/logs/bpr-main-linux-20260924/bpr-1102-1104-post-close/` and are not
committed. They are `bpr-1102-post.log`, `bpr-1102-ctest-5660cf3b.log` for
the earlier run, and `build-dev-e2dc8f82.log` for the build.

## Post-close macOS rerun (2026-09-24, integrator)

At `e2dc8f82` on macOS 26.6.2 arm64 (Apple clang, Qt 6.11.2, `build/dev`
Debug, incremental rebuild clean), `ctest --preset dev -j1`:
`application_diff_workbench_public_path` passed in 12.31 s,
`application_review_round_trip_composition_contract` in 11.29 s (the
resealed pin matches on macOS), `application_appearance_theme_customization_contract`
in 24.43 s and `application_appearance_store_fallback_contract` in 12.85 s.
This confirms AC4's code-judged macOS half by a run.
