---
schema: "implement-rfc.card.v1"
id: "BPR-1265"
title: "Windows Debug: the large-review byte-pressure course ends unready at an epoch refresh"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0967 Windows station Debug run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1264--BPR-1265--e0e97a5e10af.json","related_cards":["BPR-0967"]}
---

# BPR-1265: Windows Debug byte-pressure course ends unready at an epoch refresh

## Observation

On bpr-main-win at `682ea1392` (windows-dev Debug, MSVC 19.51, Qt 6.11.2),
`application_large_review_cache_bytes` failed after 323.46 s with
`byte-pressure stored head refresh timed out`. Its course clock was 840 s, so
the clock did not end the wait: the byte course's wait also stops when the
epoch status becomes failed, unavailable or cancelled. The same row passes on
Windows Release (280.70 s), Linux Release and Debug, and macOS. The Debug
summary keeps no phase detail, so which terminal ended it is unknown.

[[BPR-0967]] did not scale this away: it doubles Windows Debug course clocks
only because the resources course outran its 90 s clock, a different cause.
From `f61340e17` each report records `Extended.WaitExpiry`: the wait, its
clock, elapsed time, epoch status and whether the clock expired.

## Outcome

Rerun the Windows Debug row at or after `f61340e17`, read `WaitExpiry` and the
`large-review wait:` line, and attribute the ending epoch status to its owner.
Repair it, or record why Debug cannot complete the course and the owner's
decision for the row.

## Direct acceptance

- The row passes in Windows Debug, or the card records the measured cause and
  the owner's decision for the Debug row.

## Second sample: a pass — 2026-10-01

At `f61340e17` (bpr-main-win, windows-dev Debug) the row passed in 1,626.01 s,
with an empty `WaitExpiry` and no `large-review wait:` line. The 323 s ending
at `682ea1392` was not reproduced, and it happened before `WaitExpiry`
existed, so its cause is still unknown: one failure and one pass. The card
stays open for that ending.

The pass also showed the doubled Windows Debug clock (1,680 s) was too tight
for this course. From `9aa3df3bb` Windows Debug triples it (2,520 s) and the
row's timeout is 3,420 s.

## Cause found and fixed: the Git job-assignment race (bpr-main-win, 2026-10-06)

The integrating coordinator approved this on 2026-10-06 as a
security-relevant platform change.

**Reproduced** in 7 of 7 runs of the Windows Debug row at current `main`
(`build/windows-dev`):

- six ended with `epoch_status=8` (failed), at refreshes between
  `byte_epoch_refresh_5` and `_28`, 120 to 594 s into the course;
- one hung at `ready` until the 2,520 s clock expired.

**Attributed.**

- The course now relays the application's own sanitized diagnostic log
  when it fails (`large-review-diagnostic:`, at most the last 30 records that
  are not routine). The log named `epoch.failed`, stage `catalog_recovery`,
  failure `process_failure`.
- Temporary instrumentation of the Git adapter, not committed, showed the
  Git result was `process_start`, 52 to 180 ms after submit. It came from the
  unguarded Windows start: `job.attach(pid)` called `AssignProcessToJobObject`
  after the start, and that failed with `ERROR_ACCESS_DENIED`.
- At that moment `GetExitCodeProcess` read exit code 0, not `STILL_ACTIVE`.
  A short Git command, such as a ref update during catalog recovery, had
  already finished, successfully, before the parent opened it by process id
  and assigned it to the kill-on-close job. Windows refuses to assign an
  exited process.
- An optimized parent wins this race almost always. A Debug parent, slower
  still under the byte course's memory pressure, loses it. The earlier
  hypothesis, a deadline counted in the runner's queue, was wrong; no Git
  command timed out.

**Fix.**

- `src/platform/include/bpr/platform/windows_standard_handles.hpp`:
  `launch_restriction::place_in_job` names a job before the launch. `apply`
  then builds a two-entry attribute list, `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`
  and `PROC_THREAD_ATTRIBUTE_JOB_LIST`, so process creation puts the child in
  the job or creates nothing. `placed_in_job()` reports whether it did.
- The platform floor supports this: BetterPullReview ships for Windows 10/11
  x64 (`docs/development/release-packages.md`). The job-list attribute exists
  from Windows 10 (build 10240), nested jobs from Windows 8, and
  `CreateProcess` fails, rather than start the child outside the job, when
  the job cannot be applied.
- `installed_git_process.cpp`:
  - `windows_job::create()` makes the kill-on-close job before the start, and
    the unguarded Windows launch names it. A launch whose list lacks it is
    refused as before (`!inheritance.placed_in_job()`).
  - The post-start `attach` is replaced by `holds(pid)`, a check. It opens
    the started child, which Qt still holds a handle to, so the id cannot
    name another process. If `IsProcessInJob` finds it outside the job, or
    cannot check, the child is terminated and the start is refused as
    `process_start`.
  - A child Qt has already seen finish (`NotRunning`) is not checked: it was
    created inside the job.
  - Treating "the assignment failed but the child had exited" as success was
    rejected: a detached grandchild could then escape the job.
- `cmake/VerifyArchitectureBoundaries.cmake` and
  `tests/scripts/architecture_boundary_tests.py` (the boundary rule,
  resealed):
  - the Installed-Git runner contract now requires
    `inheritance.place_in_job(job.native())`, `!inheritance.placed_in_job()`
    and `IsProcessInJob(`;
  - `AssignProcessToJobObject` is forbidden in the runner;
  - each new fragment is pinned by removal in the boundary contract tests.

**The launch owners** (the coordinator asked for every owner that uses
`launch_restriction` and a kill-on-close job):

| Owner | Launch restriction | Kill-on-close job | Change |
|---|---|---|---|
| Installed Git, unguarded (`installed_git_process.cpp`) | yes | yes, assigned after the start | **this fix** |
| Installed Git, guarded by the workspace supervisor | own handle list | the supervisor's | none |
| GitHub CLI (`github_cli_process_runner.cpp`) | yes | no | none |
| Tour editor (`tour_editor.cpp`) | yes | no | none |
| Agent launch (`windows_agent_process.cpp`) and supervisor (`windows_agent_supervisor.cpp`) | own handle list | yes, but created `CREATE_SUSPENDED`, assigned, then resumed | none; no race |

**Failing-before controls.**

- `git_process_contract` gains `run_windows_finished_before_job_check_contract`.
  A fault-injection marker (`inject-delay-before-job-check`) holds the parent
  for a second after the start, so the fake `git init`, which reads no input,
  has exited before the check.
  - With the original adapter and the same seam it fails: "a Git that
    finished before the job check succeeds".
  - With the fix it passes, with 2 processes created.
- The Debug byte course itself: with the fix, every `byte_epoch_refresh` in
  two full runs completes. Before, 7 of 7 runs failed or hung in them.

**Evidence** (`build/windows-dev` Debug, MSVC 14.51, no `warning C` line):
`git_process_contract`, `git_cache_contract`, `github_cli_process_runner`,
`gh_cli_transport`, `architecture_boundaries` and
`architecture_boundary_contract` pass. The change is Windows-only code.
macOS and Linux runs of the boundary rows and the Git contracts are owed,
and the coordinator asked to be told when they are needed.

**Not yet done: the Debug row still does not pass.** With the race gone, both
runs reach the `cache_byte_selection` phases. Each selection takes 155 to
372 s, and the course clock (2,520 s) expires at selection 6 or 7 with the
epoch `ready`. The refreshes themselves take 25 to 48 s each, growing
through the course. The one recorded Debug pass, at `f61340e17`, took
1,626 s in all. This slowness is a separate cause, attributed next.

## macOS run (coordinator, 2026-10-06)

At 324f17e2c (holds 3036a08af) on macOS arm64 Debug, clean worktree: 15/15 of
git_process_contract, git_cache_contract, github_cli_process_runner,
gh_cli_transport, both architecture boundary rows, qt_workbench_title_row and
qt_workbench_window at 100/125/200, and qt_document_tabs passed.

**Note: Linux run at `324f17e2c` (holds `3036a08af`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, `build/dev` rebuilt with no warnings).** 6 of 6 passed:
`git_process_contract` (6.0 s), `git_cache_contract` (5.0 s),
`github_cli_process_runner` (1.5 s), `gh_cli_transport`,
`architecture_boundaries` (26.7 s) and `architecture_boundary_contract`
(191.0 s).
||||||| parent of feb0726c2 (BPR-1265: the byte-pressure course runs only in optimized builds; a Debug run reports Skipped with its reason)
## The Debug row: optimized builds only (coordinator decision, 2026-10-06)

The coordinator chose option (a), on every platform for consistency:
`application_large_review_cache_bytes` runs only in an optimized build. This
matches the course's own `TimingScope`, "Debug is diagnostic only".

- `large_review_benchmark.cpp`: in a non-optimized build the `--cache-bytes`
  course prints `LARGE_REVIEW_CACHE_BYTES_SKIPPED: the byte-pressure course
  runs only in an optimized build; a Debug build is diagnostic only` and exits
  77 before it makes a data root.
- `tests/end_to_end/CMakeLists.txt`: the row sets `SKIP_RETURN_CODE 77`. So a
  Debug run reports **Skipped**, which CTest's summary lists separately, not
  as a pass. This is the same mechanism `coding_format_check` uses.

Windows (`bpr-main-win`):

- `windows-dev` Debug: the row reports Skipped in 0.05 s with that line.
- The optimized tests-on tree (`build/bpr0967-release`, MSVC `/O2`): the row
  passes in 287.0 s.

The Debug presentation cost behind the decision goes to its own card,
recorded at low priority. A Debug `cache_byte_selection` ran 3,315
event-loop pulses over 135.7 s, only 528 of them with diff work. That is
GUI-thread presentation of the 20,000-line file, not the diff.
