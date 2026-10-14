---
schema: "implement-rfc.card.v1"
id: "BPR-1495"
title: "The large review launch course fails"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e7f61aa5c","evidence":{"kind":"observation","detail":"Failing check ProcessToMainMarkerMs (129.8 ms vs 100 in the cd4860e30 gate): dyld coalescing 30,190 weak definitions exported by the Debug fixture (Release app: 118). No single commit; largest steps 32ccfd8ad (RE2/Abseil) and 8172a3a63 (fmt). No budget loosened."}},{"criterion":"AC2","outcome":"pass","revision":"e7f61aa5c","evidence":{"kind":"observation","detail":"large_review_launch.py prints LARGE_REVIEW_LAUNCH_FAILED naming each failed check and its limit."}},{"criterion":"AC3","outcome":"pass","revision":"e7f61aa5c","evidence":{"kind":"command","command":"ctest --preset dev -R ^application_large_review_launch$","detail":"APPLE-only empty exported-symbols list for the Debug bench fixture (no runtime symbol lookups found): binds 30,190 -> 39, main marker median 108.1 -> 16.2 ms at load 9.9; row passed 3/3 at load 9-10 (worker) and again here at load 29.9 after one cold first launch. Under load 25-50 the fixture responsiveness and history epoch budgets can still refuse; those budgets are unchanged."}}]
---

# The large review launch course fails

`application_large_review_launch` fails on main at cd4860e30 in the full gate
and again alone, after `LARGE_REVIEW_RESPONSIVENESS_MEASURED`, without a
FAILED line naming which check failed. It also failed in the 2026-10-12 gate
(2.9 s). `application_large_review_responsiveness` and
`application_large_review_history` pass alone.

## Acceptance

- AC1: The failing check is named, with its cause and the commit that
  introduced it. A budget miss is reported as a miss; no budget or assertion
  is loosened to pass.
- AC2: If the course does not name a failing check, it is changed to name it.
- AC3: `application_large_review_launch` passes on macOS, or the remaining
  miss is reported with its measurement for the owner.

## Boundaries

No change to the course's budgets.

## Evidence

Measured on 2026-10-13 in a worktree at a8fd17aea (dev preset, Debug), while
other agents were building: load averages 21 to 41 throughout. Every timing
below is load-contaminated, so it compares builds run alternately under the
same load. It is not a quiet-machine figure.

- AC1, failing check: the `ProcessToMainMarkerMs` limit of 100 ms in
  `tests/end_to_end/large_review_launch.py`. The cd4860e30 gate's own report
  (`.claude/worktrees/release-gate/build/dev/tests/end_to_end/large-review-launch.json`)
  records `ProcessToMainMarkerMs` 129.8 against the 100 ms limit, with
  `ProcessToHomeMarkerMs` 933.6, exit code 0 and `WithinLimits: false`. The
  fixture printed its success marker, and the script then returned 1 without
  saying why. The earlier 2.9 s failure in the 2026-10-12 gate was a different
  failure that names itself
  (`large-review-benchmark-failed: selected provider/Git path or replay exhaustion`).
- Cause: almost all of the pre-main time is dyld coalescing the weak
  definitions that the Debug fixture executable exports. A `DYLD_PRINT_INITIALIZERS`
  trace shows about 126 ms passing before the first initializer, in libSystem.
  The fixture's own 12 static initializers together take under 1 ms. A
  `main` that links the same Qt frameworks reaches the marker in 13 to 18 ms
  (median). `dyld_info -fixups` counts `<weak-def-coalesce>` binds at 26,166 in
  the 2026-10-12 14:36 Debug build and 30,190 at a8fd17aea. Of the 4,024 added,
  about 2,690 are `absl::`/`re2::` and about 630 are `fmt::`. The Release
  `BetterPullReview` executable (2026-09-25 build) has 118, so the shipped app
  does not pay this cost: it is a Debug-fixture cost, not a product launch
  regression.
- Introducing commits: there is no single one. The margin has worn away as the
  composition grew. At af7144da1 (v0.1.10) the row measured 82.7 ms on a quiet
  machine and already failed once under gate load. 32ccfd8ad (BPR-1431, RE2
  and Abseil linked through the regex adapter) is the largest single step,
  and 8172a3a63 (BPR-1439, fmt) is the second.
- Measurements: three alternating rounds of 15 launches each, timed to
  `LARGE_REVIEW_MAIN_ENTERED` at load averages of about 35. Medians: af7144da1
  99.9, 97.9 and 96.8 ms; the 2026-10-12 14:36 build 101.8, 100.0 and 98.8 ms;
  a8fd17aea 116.9, 112.4 and 114.0 ms. Three CTest runs at a8fd17aea
  before the script change (load averages 37, 31 and 28) measured
  `ProcessToMainMarkerMs` at 2313.5, 115.1 and 112.0. Three after it (load
  averages 26, 27 and 25) measured 137.5, 134.5 and 188.6, and each run named
  its refusal.
- Budget miss: `ProcessToMainMarkerMs` misses its 100 ms limit at a8fd17aea under
  the load measured here, and it measured 129.8 ms in the cd4860e30 gate. No
  quiet-machine figure was available. Scaling af7144da1's quiet 82.7 ms by
  the step measured under load (about 15 ms) puts a8fd17aea near 95 to 100 ms
  quiet, so the row has no margin even on a quiet machine. No budget or
  assertion was changed.
- Remedy, applied on the integrator's decision: on APPLE only,
  `tests/end_to_end/CMakeLists.txt` links `bench_better_pull_review_large_review`
  with an empty exported-symbols list
  (`LINKER:-exported_symbols_list,<build>/tests/end_to_end/large_review_no_exports.txt`).
  The 100 ms limit stands for the shipped app's launch. The Debug fixture's
  30,190 weak-coalesce binds, against 118 in the Release app, are a cost that
  only the Debug test build pays. Removing that cost leaves the limit
  unchanged, and the row still measures process start to the main marker.
  Before the change, a grep of `tests/end_to_end` and `src/app` for `dlsym`,
  `dlopen`, `rdynamic`, `export_dynamic`, `Q_IMPORT_PLUGIN`,
  `qt_import_plugins`, `STATIC_PLUGIN` and `ENABLE_EXPORTS` found nothing, so
  nothing looks up symbols in this executable at run time. After the relink
  the binary has 39 weak-coalesce binds.
- Paired probe at a load average of 9.9 (15 launches to the main marker): an
  exporting build of the same fixture had a median of 108.1 ms (minimum
  105.9). The non-exporting build had a median of 16.2 ms (minimum 15.4).
- AC2: `large_review_launch.py` now prints one
  `LARGE_REVIEW_LAUNCH_FAILED: <check>` line for each failed check (the
  fixture's exit code, a missing marker, marker order, and each enforced
  limit with its measurement). The pass and fail conditions are unchanged.
- AC3, met. After the change, each row that runs this executable was run
  once, one after another, and `application_large_review_launch` three times.
  - `application_large_review_launch` at load averages of 24, 32 and 34:
    main marker 25.1, 27.4 and 22.6 ms, within its limit. Every run failed
    because the fixture's own responsiveness budget refused under that load
    (input and completion misses such as `paged_review_and_git_load_input_gap`
    and `files_drawer_open`; the fixture exited 1).
  - Re-run at load averages of 9.3, 10.3 and 8.9: all three passed, with main
    marker 15.9, 16.1 and 16.2 ms and home marker 659.9, 663.7 and 663.5 ms.
  - Passed once each (load averages 27 to 52): `application_large_review_benchmark`,
    `_timing_control`, `application_unavailable_diff_public_path`,
    `application_approximate_diff_public_path`, `application_first_tab_comparison`,
    `application_large_review_responsiveness`, `_active_work`, `_provider_cancel`,
    `_history_session_race`, `_cache_entries`, `_prepared_open`, `_loaded_close`
    and `_resources`. `_cache_bytes` is skipped in a Debug build, as designed.
  - `application_large_review_pathological` (load 30, input budget miss on
    `paged_review_and_git_load`) and `application_large_review_history` (load
    52, `epoch refresh exceeded its measured platform budget`) refused under
    load. Both passed when re-run at a load average of 10, in 4.2 s and
    151.5 s.
  - `large_review_replay_contract` runs a different executable
    (`better_pull_review_large_review_replay`), so it is not affected. That
    executable was not built, and the row was not run.
  - No budget was changed.

- Integration at e7f61aa5c (combined with BPR-1499 over main 23b0deeac), macOS arm64: after the relink, `application_large_review_launch` failed once on a cold first launch (main marker 1310.9 ms, home 1983.9 ms, load 35.7) and then passed (10.9 s, load 29.9). In the gate, `application_large_review_benchmark` runs the same executable before this row.
