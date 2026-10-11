# Release test runtime and regression corrections

[[BPR-1386]] and [[BPR-1387]] investigate the owner's 74-minute macOS
release gate and repair the failures it exposed. This is local test evidence,
not package signing, publication or another platform's qualification.

## Where the 74 minutes went

The retained `build/release-0.1.7-tests.log` contains 685 CTest rows: 676
passed, eight failed and one was explicitly skipped. Wall time was 4,450.03
seconds (74.17 minutes), while summed test durations were 4,449.54 seconds.
That near equality shows the suite ran sequentially. The top 20 rows took
2,136.64 seconds, or 48% of the total. All 98 core tests together took 68.21
seconds; 356 passing rows below one second together took 147.49 seconds.

| Historical row | Seconds | Main cost or failure |
|---|---:|---|
| `application_composition_contract` | 503.31 | A new tests-disabled Release build on every invocation; stale source-manifest assertion also failed |
| `application_profile_window_review_restoration_public_path` | 196.54 | Composed application restoration course |
| `diagnostic_context_age` | 180.17 | Intentional real three-minute context-expiry wait |
| `application_large_review_history` | 154.54 | 32 recorded history epochs; input-gap budget failed |
| `architecture_boundary_contract` | 128.61 | Positive and adversarial architecture checks |
| `application_review_queue_enrollment` | 103.14 | Composed queue course |
| `dependency_modernization_contract` | 102.32 | Dependency/configuration contracts |

The count of small tests is not the main cost. No complete test family was
shown redundant enough to delete. Core decisions, provider contracts, composed
public paths and tests-disabled release absence checks establish different
claims. The improvements remove repeated compilation and unnecessary serial
scheduling while retaining every registered row and its assertions.

## Changed procedure

`tools/release.sh` runs `python3 scripts/run_test_suite.py --preset dev`.
Four independent tests may run concurrently; Qt and end-to-end courses run
alone. Existing isolation, fixture dependencies, resource locks, timeouts and
performance budgets remain in force. The runner checks the selected inventory
against completed JUnit rows, retains failures and skips, and fails on missing
or duplicate results. It also refuses a manifest that would run an interactive
course concurrently.

A checkout-wide file lock refuses overlapping complete-runner invocations.
Direct `ctest` commands do not acquire it. During this investigation a separate
full `ctest --preset dev` was found running in the same build tree; the new
run's scheduler was paused, allowing its current child to finish, until that
earlier run ended. The final measurements record that pause separately, and
failures from the overlap require focused checks afterward.

The composition contract reuses its own tests-disabled build tree beneath the
CTest binary directory. Every invocation still configures, builds, inspects
the linked graph and exercises the resulting executable. Only compiler outputs
are cached. Standalone invocation still uses a disposable tree unless a build
directory is explicitly supplied.

Logs and machine-readable reports remain under ignored `build/`. Reproduce a
historical report with `scripts/run_test_suite.py --report-log`, or obtain a
complete new report with `--preset dev`. Wall time is the elapsed gate time;
summed test time includes overlapping rows and is not an elapsed-time estimate.

## Regression investigation

The original eight failures were checked against the current source before
claiming fixes. The round-trip composition digest already passed; its shared
digest assertion had been corrected since the retained release log. Active-work
responsiveness also passed the first current-source run. Launch and history
still exceeded the 100 ms input-gap limit in Debug (112.45 and 117.51 ms).

Three workflow failures reproduced. Home's metadata fixture intercepted the
complete PR read at a shared endpoint and returned a partial body. Background
metadata reads could also consume the ordered snapshot recording. The fixture
now recognizes snapshot reads by their exact request identity after their
repository read, serves configured Home metadata separately, and refuses
unsupported metadata without disturbing the snapshot cursor. Both kinds of
request still undergo canonical wire validation.

The rate-limit test evaluated its readiness predicate repeatedly after a
successful wait. A background job could start between those evaluations,
causing a false failure before the deadline. The wait now returns at the first
successful observation and emits diagnostic state on an actual timeout.

The full pass also exposed a course-boundary race in personal discovery. The
CLI path completed six identity checks from its preceding PR-window course
after the next course had taken its no-read baseline. The previous window now
waits for its admitted epoch and workbench work before closing. Both native and
CLI courses pass, preserving the assertion that an empty Home window performs
no account or vault read.

Profiling initial snapshot delivery found a second, unnecessary copy and
projection of all conversations in the application composition. When no source
is selected, comment placement returned exactly the host's original projection.
That path now uses the original projection and clears the placement cache;
source-specific placement still reprojects and preserves the Markdown context.
The epoch handoff then cleared a nonexistent initial comparison and republished
the conversations again. Clearing now reprojects only when a comparison had
actually been displayed; clearing a real source still removes its placement.
The benchmark also leaves fixture-server locking out of its event-loop poll;
the existing post-load verification still requires exactly one Git fetch and
all provider, identity, content and exhaustion assertions.

## Complete integration measurement

Measured on this Mac: M4 Pro, 14 cores, 48 GiB, Apple clang 21, Qt 6.11.2,
`dev` Debug. Build time before CTest is excluded in both measurements.

| Measurement | Historical release | Updated complete pass |
|---|---:|---:|
| Registered/completed CTest rows | 685 | 687 |
| Passed / failed / skipped | 676 / 8 / 1 | 679 / 7 / 1 |
| Actual wall time | 74.17 min | 62.31 min |
| Explicit scheduler pause | none recorded | 12.60 min |
| Wall time excluding that pause | 74.17 min | 49.71 min |
| Sum of test durations, including overlap and pause | 74.16 min | 82.80 min |
| Tests-disabled composition contract | 503.31 s | 414.29 s cold; 42.97 s warm |

The 49.71-minute figure is elapsed time with the documented 755.82-second pause
subtracted, not a clean isolated benchmark. The earlier overlap also affected
some courses. The complete pass preserved all 687 manifest entries: 341 may
overlap and 346 remain serial. It returned failure, as required. Its seven
failures remain in the saved log and JUnit report; subsequent focused results
do not rewrite that history. The existing `application_large_review_cache_bytes`
Debug skip remains a skip; this work claims no new cache-memory qualification.

The warm composition check saves 371 seconds compared with this pass's cold
check, and 460 seconds compared with the historical cold row. Substituting that
warm duration into this run would give about 43.5 minutes excluding the pause;
that is an estimate, not a second measured complete-suite result.

The review-start and review-progress-push courses passed unchanged on the
exclusive recheck (11.59 and 4.64 seconds). Their earlier failures occurred
during the overlapping full suites, including a `PROFILE_ALREADY_OPEN`
refusal. The canary's timeout included the paused scheduler; its independent
recheck passed in 86.89 seconds. Personal discovery required the settling fix
above: native 19.43 seconds, CLI 7.91 seconds.

## Evidence retained locally

The full run is `build/test-suite-optimization/full-run/`: `manifest.json`,
`ctest.log`, `junit.xml` and `timings.json`. `contention.json` in the parent
directory records the pause. Historical summaries are `baseline-timing.json`
and `baseline-timing.txt`. Focused logs and profiling diagnostics are in that
same ignored directory. They contain synthetic fixture activity, not release
signatures or publication evidence. This document is the committed summary;
transient logs and build outputs are not committed.

## Final focused acceptance

All eight originally failing rows pass on the final source, with no test
deleted, new skip, raised performance limit or automatic retry-to-green policy:

| Historical failure | Final result |
|---|---|
| `application_composition_contract` | Pass, 42.97 s warm, including all eight internal contracts and adversarial bypass cases |
| `application_review_round_trip_composition_contract` | Pass, 3.51 s |
| `application_large_review_launch` | Three consecutive passes, 11.14 / 11.11 / 11.17 s |
| `application_large_review_active_work` | Pass, 9.61 s |
| `application_large_review_history` | Pass, 142.61 s; all 32 epochs and 31 selections |
| `application_home_open_feedback_public_path` | Pass, 59.61 s |
| `application_rate_limit_render_recovery` | Pass, 25.00 s |
| `application_active_changed_scaling` | Pass, 19.97 s |

The final launch report's maximum load-phase input gap is 96.42 ms against
the unchanged 100 ms limit. This is evidence for this hardware/configuration,
not a universal latency guarantee. The pathological-file course also passes
(3.71 s). Final native/CLI discovery, review-start, review-progress-push,
threaded replies, surviving reply drafts, composed Markdown links, window-read
host, release-tool contract and suite-runner contract checks pass.

The 16-row final focused run first reported 15 passes and the expected pinned
source-digest drift caused by the production optimization. After the reviewed
pin update, the complete composition contract passed independently. Logs are
`final-focused.log`, `composition-final.log`, `performance-final-active.log`
and `launch-final-stability.log`. Earlier failed diagnostic attempts remain
retained; none is turned into a passing complete-run result. The final fixes
received focused verification, not a second complete 687-row benchmark.

Engineering self-review of the production diff against `c19d159e4` confirms
that only `github_application.cpp` changes within the pinned production
manifest. Both optimizations reuse existing presentation: absent source means
placement previously returned the original host projection, and the only reset
of `SelectedDiffPublication` remains `clear_diff_selection`, which still
reprojects when that publication existed. No authority, provider, credential,
mutation or source inventory changes. The full-application and manifest pins
are advanced for this reviewed diff; the separate production mutation pin is
unchanged. The adversarial pin/bypass checks remain enabled.

Final architecture boundaries pass (26.52 seconds), all five changed C++ files
pass clang-format 22.1.8, and `git diff --check` passes. The documentation gate
reports `DOCUMENTATION_OK`; all 67 documentation contract tests pass. Both
corrective cards pass scoped board validation. The completed `dev` build and
full pass's unchanged architecture adversarial checks remain supporting evidence.
