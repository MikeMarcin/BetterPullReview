---
schema: "implement-rfc.card.v1"
id: "BPR-1598"
title: "The coding-tidy gate passes, and the large-review timing rows hold on a loaded machine"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved cleanup, 2026-10-17","related_cards":["BPR-1373","BPR-0967","BPR-1247","BPR-1495","BPR-1580","BPR-1583","BPR-1560","BPR-1557"],"reservation":"docs/issues/_reservations/BPR-1598--BPR-1598--95fbc5d6a30d.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9252502e4","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; PATH=llvm@22 tidy.py --build-dir build/dev --warnings-as-errors over the touched translation units; clang-tidy 22 over all 885 gate translation units","detail":"The two findings are fixed (home_panel.cpp summary flags made int explicitly; server_card.cpp using_ renamed usedHere) with four more trivial ones in the same adapters (device-authorization Glaze option names marked as the sibling contracts mark them, noteLimit, the merged get_merge_base/timeline case, process_supervision.hpp include guard). Format check passes over 1,608 files; tidy passes on every changed translation unit (the benchmark's own findings are pre-existing, none on a changed line). The repo-wide gate is not green: 361 other findings in 109 files remain, listed below as AC1 allows; qt_home_list, qt_workbench_home_discovery, qt_composed_github and _home_sign_in, qt_anonymous_public_preview, qt_disclosure_button, application_saved_server_contract, github_read_contract, github_device_authorization_contract, github_paged_read, architecture_boundaries, architecture_boundary_contract and test_network_isolation_manifest pass (20/20)."}},{"criterion":"AC2","outcome":"pass","revision":"9252502e4","evidence":{"kind":"command","command":"ctest -R large-review rows, load average 68-117","detail":"Before: at load 75-78 responsiveness, launch, active_work, pathological, history and large_review_replay_contract (timeout at 33 s) failed. After: timed figures are attributed (GUI-thread intervals less that thread's own sampled wait, floored by its processor time; completions less every thread's wait, floored by the busiest thread's processor time); all rows pass at load 68-117, limits unchanged, wall readings kept in each report. Residual misses come from the GUI thread blocking on malloc's fork lock while a preempted worker starts Git (sampled: _xzm_fork_lock_wait under github_application::status), so a timing miss while the GUI thread measurably waited is run once more by large_review_course.py, keeping the first report. The replay contract's budget is now 30 s of processor time (RLIMIT_CPU); it used 16.6 s CPU in 67 s wall at load 111."}},{"criterion":"AC3","outcome":"pass","revision":"9252502e4","evidence":{"kind":"command","command":"application_large_review_timing_control; temporary 150 ms stall in workbench_drawer_panel::apply_visibility (not committed); replay --processor-seconds=5","detail":"Timing control with 28 spinning threads: 120 ms of GUI work reads 120.2 ms (wall 732.9), 40 ms reads 40.1 (wall 178.7), a 120 ms sleep reads 125.2 with zero thread wait (process-wide wait was 51.7, which is why the GUI wait is sampled per thread). Injected 150 ms sleep and 150 ms spin on drawer open at load 67-69: responsiveness fails, is retried once, fails again (files_drawer_open 195.7 and 202.9 ms against 100; drawers_show 338.5 and 368.1). Replay with a 5 s processor budget stops with LARGE_REVIEW_REPLAY_CPU_LIMIT."}},{"criterion":"AC4","outcome":"pass","revision":"9252502e4","evidence":{"kind":"command","command":"ctest --preset dev -j6 with twelve processor burners, stopped at 725/775 under the owner's one-card-at-a-time policy; python3 scripts/verify_documentation.py --root .","detail":"Load average 43-120 through the run. Every large-review row, the replay contract and the timing control passed; application_large_review_pathological passed on its retry (first attempt: paged_review_and_git_load gap 108.7 ms with the GUI thread waiting 40% of the course). The run's other failures (live_mutation_isolation and application_composition_contract timeouts, release_update_native_https, qt_composed_long_thread_scale_125, application_threaded_reply_contract, application_epoch_portable_occurrence_two_catalogues) are rows this card does not touch. The owner's policy moved the complete full-suite proof to the final integrated suite. DOCUMENTATION_OK."}}]
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
  22) passes over every translation unit; any other finding it reports is
  fixed when trivial and behaviour-neutral, or listed here.
- AC2: Every timed figure a large-review limit reads leaves out the time the
  course was kept off a processor by another process, so a loaded machine no
  longer fails the rows; the limits themselves are unchanged and the wall
  readings stay in each report.
- AC3: A genuine regression still fails: the timing control proves that GUI
  work and a blocked GUI thread still count in full while the processors are
  saturated, and an injected stall in the product fails its row under load.
- AC4: Under a loaded `ctest --preset dev -j6` none of these rows fails; the
  documentation gate passes. (The owner's 2026-10-17 landing policy moved the
  complete full-suite proof to the final integrated suite; this card records
  a run stopped at 725 of 775 rows.)

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

## Repo-wide clang-tidy 22 findings left in place

The gate run over all 885 translation units reports 361 findings in 109 files
besides those fixed here, far more than the two this card was opened for; most
rows land through tidy runs over touched files only. By check:
readability-identifier-naming 216, performance-avoid-endl 20,
modernize-use-emplace 18, readability-static-accessed-through-instance 12,
bugprone-throwing-static-initialization 12,
performance-inefficient-string-concatenation 11,
bugprone-optional-value-conversion 9, clang-diagnostic-error 8 (clang-tidy
22 cannot parse Glaze's `decay_keep_volatile_t` in
`installed_git_process.cpp` and `syntax_refinement_cancellation_tests.cpp`),
performance-unnecessary-copy-initialization 7, float loop counters 10,
portability-avoid-pragma-once 4, and 31 more across 20 checks. By area:
tests/end_to_end 63, tests/github_contract 46, tests/adapters_qt 41,
tests/sqlite_contract 37, tests/application 34, src/application 23, src/app
27, src/adapters/sqlite 18, tests/core 16, src/core 11, src/adapters/qt 10.
Not behaviour-neutral as a sweep (`std::endl` flushes markers; several are
possible bugs), and touching 109 files would collide with the worktrees in
flight, so they are listed rather than fixed. Worth a look first:
`src/app/main.cpp:10218` (use after move of `fixtureName`),
`tests/sqlite_contract/private_review_storage_tests.cpp:407` (a length of 14
over a 13-byte literal), `tests/sqlite_contract/appearance_settings_storage_tests.cpp:805`
(two statements joined by a missing comma, perhaps on purpose). The gate also
runs whichever `clang-tidy` is first on `PATH`; on this Mac that is LLVM 23,
whose new checks add many more.

## Next

Land after "Finish BPR-1572" is on main. The full-suite proof under load runs
with the final integrated suite.
