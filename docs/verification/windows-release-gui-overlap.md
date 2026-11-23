# Windows Release GUI rows overlapped by measurement

2026-11-21. The complete Windows Release suite spent about two thirds of its
row time in rows that run alone: 467 serial rows took 112 of 181 summed
minutes in the full 0.5.0 run of 18 November. Most are serial only because
`bpr_schedule_test_directory` makes every row under `tests/adapters_qt` and
`tests/end_to_end`, and every row with a `QT_QPA_PLATFORM`, exclusive by
default. The audited routes that were meant to free some of them
([BPR-1670](BPR-1670-windows-suite-runtime.md),
[BPR-1673](BPR-1673-command-owned-correctness.md)) bind each family to hashes
of its test source and of product files. Every family was stale on `main`, so
no row overlapped in any 0.5.0 Release run.

This record is the measurement behind the Windows Release route in
`cmake/BprTestScheduling.cmake` (`bpr_overlap_unclassified_gui_rows`). It is
a measurement of one machine and one source revision, not a source audit.

## What ran

- Host: the Windows release PC, Intel Core i9-10900KF, 20 logical processors,
  Windows 11, otherwise idle. Interactive Limited scheduled task, the context
  the release rounds use.
- Tree: its own clone at `C:\code\bpr-gui-overlap`, outside the release
  roots, under the same Defender exclusion the release build tree has.
- Source: experiment commit `4ff0922b6`, which is `main` at `084b8ff88` plus
  a configure option that removes `RUN_SERIAL` from rows that are serial only
  by the default. Explicitly serial rows stayed exclusive and the audited
  resource locks were kept. Visual Studio 18 2026, preset
  `windows-qualification-release`, Release, built at 8 jobs.
- Selection: `ctest -L bpr_experiment_overlap -E '^application_composition_contract$'`,
  516 rows.

| Pass | Jobs | Wall | Summed row time | Failed |
|---|---:|---:|---:|---:|
| Overlapped A | 4 | 28.9 min (1731 s) | 114.1 min | 10 |
| Overlapped B | 4 | 27.0 min (1619 s) | 107.9 min | 6 |
| Serial control | 1 | 78.0 min (4678 s) | 78.0 min | 0 |

All three passes ran the same binaries. 505 of the 516 rows passed both
overlapped passes, and every row passed alone. Those 505 rows sum to 73.4
minutes alone and 105.7 minutes in overlapped pass A: a row takes longer when
it shares the machine, and the set still finishes in a little over a third
of the serial time.

## Rows kept serial

Failed both overlapped passes:

- `application_cpp_identifier_appearance`
- `application_enhance_comment_note_public_path`
- `application_local_comments_composition_public_path`
- `application_personal_discovery_public_path`
- `application_setup_destination_public_path` (timeout)

Failed one of the two:

- `application_automated_run_budget`
- `application_dependable_session_public_path`
- `application_personal_discovery_cli_public_path`
- `application_review_submission_composition_public_path`,
  `application_review_submission_composition_scale_125` and
  `application_review_submission_composition_scale_200` (timeouts)

Kept serial without a failure:

- `application_composition_contract` builds a nested Release tree up to eight
  jobs wide. It was not in the measured selection.
- `application_source_pr_search_performance` is a dedicated performance
  course.
- A row whose `QT_QPA_PLATFORM` is anything but `offscreen` shares the native
  desktop. `application_windows_title_row` is the one such default-serial row
  today; it passed both passes and stays exclusive all the same.

All eleven passed in the serial control, so each fails only when it shares
the machine. Whether that is a shared resource or a deadline missed under
load is not diagnosed. The three `review_submission_composition` rows took
about 50 seconds alone and reached their 90-second timeout when overlapped.

## What the landed schedule is

At the landing commit the real Windows manifest has 1,037 rows. In Release 504
may overlap by this measurement and 62 are serial; `scripts/run_test_suite.py`
accepts that manifest. In Debug 540 rows are serial and 26 are admitted by the
BPR-1670 allowlist, as before.

## Not established

- Two overlapped passes are a small sample. A row that collides rarely can
  have passed both; it will show as an intermittent failure and belongs on
  `KEEP_SERIAL` when it does.
- No complete suite has run with the landed schedule, so the saving for a
  whole release run is not measured. The next Windows release run is that
  measurement.
- macOS, Linux and every Debug graph were not measured and are unchanged.
- A row added after this measurement overlaps in Windows Release without
  having been measured. One that needs the machine to itself must say
  `RUN_SERIAL TRUE`.

## 0.5.2 release follow-up

The Windows build 91 normal Release run overlapped
`application_home_open_responsive_public_path` with other rows. The course
left Home for the requested pull request in 49 ms and painted its Overview
in 54 ms. It failed when it checked whether the provider worker had already
entered the artificial hold. Build 85 had the same failure after a 28 ms
gesture and 32 ms first paint.

The course records that its first Overview frame still awaits the header,
then waits separately for the fixture worker to enter the hold. Its two
100 ms Release requirements still cover the first frame and the longest
GUI turn before that frame. `KEEP_SERIAL` in `tests/CMakeLists.txt` keeps the
course exclusive in the measured Windows Release schedule.

## Evidence

The three JUnit reports and the run summary are kept outside source under
`/Users/jaeger/.codex/release-runs/gui-overlap-20261121/`. The experiment
tree and its scheduled tasks were removed from the PC after the run.
