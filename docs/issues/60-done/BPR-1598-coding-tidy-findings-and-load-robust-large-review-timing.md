---
schema: "implement-rfc.card.v1"
id: "BPR-1598"
title: "The coding-tidy gate passes, and the large-review timing rows hold on a loaded machine"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved cleanup, 2026-10-17","related_cards":["BPR-1373","BPR-0967","BPR-1247","BPR-1495","BPR-1580","BPR-1583","BPR-1560","BPR-1557"],"reservation":"docs/issues/_reservations/BPR-1598--BPR-1598--95fbc5d6a30d.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0bea4e26d","evidence":{"kind":"observation","detail":"Native coding-format-check and coding-tidy complete with exit 0: 1,628 files and all 899 translation units, LLVM 22. Tidy helper contract covers four cases. Accepted visual designs preserved; frontend-design skill loaded."}},{"criterion":"AC2","outcome":"pass","revision":"0bea4e26d","evidence":{"kind":"observation","detail":"Unchanged limits use measured scheduling attribution, with wall readings retained. Loaded-machine large-review batch passes 15 executed rows; Debug cache-byte row skips as documented. Windows retains its documented wall fallback."}},{"criterion":"AC3","outcome":"pass","revision":"0bea4e26d","evidence":{"kind":"observation","detail":"Fresh timing control counts 120.19 ms GUI work within 585.28 ms wall, 40.22 ms within 152.15 ms wall, and 125.10 ms blocked GUI. Fresh five-second replay CPU negative control fails with LARGE_REVIEW_REPLAY_CPU_LIMIT. Unchanged product-path sleep/spin negative controls at 9252502e4 each fail both loaded attempts."}},{"criterion":"AC4","outcome":"pass","revision":"0bea4e26d","evidence":{"kind":"observation","detail":"Affected targets build. All 84 focused rows pass (1212.24 s), then all nine final residual/composition checks pass (201.40 s). Large-review batch passes under concurrent analyzer load (592.19 s; one documented Debug skip); responsiveness/history each use one bounded retry. Documentation and diff checks pass. Integrated full suite remains assigned to handoff item 9."}}]
---

# BPR-1598: The coding-tidy gate passes, and the large-review timing rows hold on a loaded machine

Two pieces of upkeep the owner approved on 2026-10-17.

**The tidy gate.** `cmake --build --preset dev --target coding-format-check
coding-tidy` stopped on findings no card owned: a narrowing conversion in
`home_panel.cpp`'s summary flags, and the `using_` lambda in
`server_card.cpp` ([[BPR-1373]]), whose name the naming rule refuses.

**The timing rows.** `application_large_review_responsiveness`, `_launch`,
`_active_work`, `_pathological`, `_history` and `large_review_replay_contract`
fail when other builds load the machine and pass when rerun alone
([[BPR-1580]], [[BPR-1583]], [[BPR-1560]], [[BPR-1557]]). Their limits are
wall-clock limits; the rows already carry `RUN_SERIAL`, which keeps CTest from
running its own rows beside them but cannot hold back another agent's build.

## Acceptance

- AC1: The two findings are fixed without changing behaviour, and the full
  `coding-format-check` and `coding-tidy` gate (clang-format 22, clang-tidy
  22) passes over every translation unit. The ordered handoff requires the
  complete gate to be green, superseding the earlier inventory-only plan.
- AC2: Every timed figure a large-review limit reads leaves out the time the
  course was kept off a processor by another process, so a loaded machine no
  longer fails the rows; the limits themselves are unchanged and the wall
  readings stay in each report.
- AC3: A genuine regression still fails: the timing control proves that GUI
  work and a blocked GUI thread still count in full while the processors are
  saturated, and an injected stall in the product fails its row under load.
- AC4: Under external load the focused large-review rows pass, as do the
  affected code’s focused tests and documentation gate. The owner’s ordered
  handoff assigns the one full integrated suite to item 9, after BPR-1599.

## What changed

- `tests/end_to_end/large_review_native_memory.cpp` gains the scheduling
  clocks: the calling thread's processor time, the observed GUI thread's
  scheduler wait (macOS samples its run state and processor time every
  millisecond, because the kernel keeps runnable time only per process;
  Linux reads `/proc/thread-self/schedstat`; Windows measures none), the
  process's total wait, and every thread's processor time.
- `large_review_benchmark.cpp` attributes every timed figure a limit reads
  (see the fixture README's "Timing under external load"), keeps the wall
  readings in each report (`Phases`' `Wall*`, `ThreadWaitMs`,
  `ProcessWaitMs`, `MaxGap*`; `Extended.Schedule`), holds one-input phases to
  the GUI-thread figure, and marks a timing miss under load
  `LARGE_REVIEW_TIMING_RETRYABLE`. The timing control adds two contended
  phases.
- `large_review_course.py` runs the course rows and gives such a miss one
  more attempt, keeping `REPORT.first-attempt.json`; `large_review_launch.py`
  attributes its markers and retries the same way. History's timeout is 1200 s
  to leave room for that attempt; no limit changed.
- `large_review_replay_contract` runs on a 30 s processor-time budget
  (`RLIMIT_CPU`, 60 s on Linux) with a ten-times wall backstop; Windows keeps
  its 120 s wall timeout.
- `RUN_SERIAL`, which the rows already had, stays: it keeps CTest's own rows
  away, which no attribution can, while attribution handles other processes.

## Completion of the full gate

The handoff inventory covered 885 translation units. After merging main
`57f335411` at `e569d67aa`, the fresh LLVM 22 inventory covered 899 units;
121 runs reported findings, including findings in shared headers. The cleanup
covers naming, explicit static calls, fixed-size constant schema tables,
integer loop counters, direct optional conversions and fewer temporary
allocations. Copies that must survive an erase or UI update retain narrow,
explained suppressions. Home’s two supported list types explicitly instantiate
all virtual overrides in `home_list.cpp`; three narrow annotations account for
the analyzer’s inability to see those definitions from callers. Glaze option
names and reflected fixture keys keep
their required spelling. Output markers retain explicit flushing.

The migration payload now derives its length from its array bounds and keeps
both NUL bytes. Its two-statement malformed-value mutation remains one SQL
call. The app retains the fixture name for the later attended read and passes the
optional tour-stop body directly to the fixture staging method. Benchmark
arguments use checked integer conversion. These details receive their existing
focused storage, app and benchmark coverage.

The tidy helper now discovers LLVM 22 by default and supports bounded parallel
read-only analysis with `CODING_STANDARD_TIDY_JOBS` or `--jobs`. Parallel
`--fix` is refused because translation units share headers. Four Python cases
cover tool selection, failure aggregation, concurrency and option validation;
the registered `coding_standard_tidy_contract` passes.

The `src/app` changes have been reviewed directly for this ordered handoff.
The dated composition reseal covers the fixture name, optional assignment,
fixture lambda names, baseline reservation and unknown-stage refusal. The
application and production mutation pins remain unchanged; only the app
manifest pin changes. The frontend-design skill was loaded for the Qt surface
work; accepted visual designs are preserved.

## Verification

The full format gate passes across 1,628 files. The changed test targets
and focused consumers build, including the final workbench test file. The full
tidy gate passes all 899 translation units with LLVM 22. All 84 focused rows pass in 1,212.24 seconds; nine
final checks also pass, covering the follow-on edits and both composition
contracts on `0bea4e26d` (201.40 seconds). The
16-row large-review
batch completed in 592.19 seconds: 15 passed, and the documented Debug-only
cache-byte row skipped. History passed in 413.43 seconds while the analyzer
was running. Responsiveness and history each used their single bounded
retry after a timing miss with measured GUI scheduler wait (3,538 ms of a
15,380 ms course and 15,500 ms of a 200,972 ms course respectively). The
first-attempt reports are retained by the runner. All other rows passed on
the first attempt.
The fresh timing control records 120.19 ms of GUI work despite 585.28 ms wall
time, 40.22 ms of GUI work despite 152.15 ms wall time, and a blocked GUI
interval of 125.10 ms. The fresh replay negative control
with `--processor-seconds=5` fails with `LARGE_REVIEW_REPLAY_CPU_LIMIT` as
required. The inherited product-stall negative controls at `9252502e4` remain
applicable: temporary 150 ms sleep and spin delays on drawer opening failed
responsiveness on both attempts under load; neither delay was committed.
The timing budget logic and injected product path have not changed since that
evidence. No new full-suite run is made for this card; item 9 owns it.

The diff benchmark accepts `3 0` and refuses a non-integer repetition count
and negative unsigned line count. The documentation gate passes.

The first complete native gate exposed follow-on diagnostics after the initial
fixes. All six affected translation units pass their direct recheck, including
a caller of the explicitly instantiated Home list types. The final full gate
passes; the verified app-manifest pin is
`235e94e1a255863ab4e3ebdf4150ca3e5ff7191776b51df00634a26cc57bdef1`.

## Integrated follow-up — 2026-10-18

Handoff item 9 checks 902 native translation units. Two later BPR-1589 changes
at `39afb7162` introduce a local naming violation (`most_teams`) and an identical
`return false` switch branch for team membership. The local is renamed and the
cases grouped without changing behavior. Both units pass the focused tidy22
recheck, reusing the other 900 passing units from the full pass. Full format22
passes 1,632 files; the two repaired files pass again. Both affected targets and
the final application relink pass; `github_read_contract` and `github_provider`
pass in 3.96 seconds. The full command's initial exit 1 and the focused recheck's
exit 0 are retained in the handoff progress report; no second full pass is
claimed. The 91 units absent from this Mac's compilation database remain outside
this native result.
