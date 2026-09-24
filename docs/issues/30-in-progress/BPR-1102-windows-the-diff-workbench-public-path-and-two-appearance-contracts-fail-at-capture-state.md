---
schema: "implement-rfc.card.v1"
id: "BPR-1102"
title: "Windows: application_diff_workbench_public_path and the two appearance contracts (theme_customization, store_fallback) fail at DIFF_INTERACTION_FAILURE:capture-state"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's application sweep","found_at":"bcd57840","first_seen":"b9991925 (BPR-1022's rows)","cross_platform_at":"5660cf3b","related_cards":["BPR-1033","BPR-1073","BPR-1050","BPR-0265"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
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
