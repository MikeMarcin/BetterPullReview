# BPR-1184 large-project responsiveness — 2026-10-04

[[BPR-1184]] measures Review Cycles on large projects for RFC-BPR-0054 RC54-08
and RFC-BPR-0061 RC61-10, against the
[LIMITS contract](../rfcs/assets/bpr-review-cycles-v1/specs/LIMITS.md): cold and
warm open, browsing, capture, typing and navigation while a capture runs,
comparison, freeze and export, and peak memory, on named corpora, with the
limits a reviewer meets stated rather than silently applied. The human
steering comparison, RC61-10's other half, needs a person and is asked
separately (see "Human steering benefit" below).

## Method

**The course.** `application_review_cycle_large_project`
(`tests/end_to_end/review_cycle_large_project_contract.py`) drives the
application's own profile desktop, the production composition the
[[BPR-1166]] course drives one window-system gesture at a time
(`review_cycle_two_round_public_path_tests.cpp`), and the shipped `bpr cycle`,
one process per command. Each desktop gesture is timed inside the process,
from the gesture to the condition it waits for, and reports the GUI thread's
longest single event-loop turn while it ran ("busy"): an input event arriving
meanwhile would have waited at most that long. A terminal command is timed as
a person runs it, process start to exit, with that process's own peak memory
(`wait4` resident set on POSIX, peak commit on Windows).

**Phases.**

- *Key typed while capturing.* Return opens a note on a selection, Capture
  revision is clicked, the note is clicked again, and one key is due every
  40 ms until the capture ends. A key's latency is from when it was due to
  when the window had handled it. The note must then hold exactly the keys
  typed.
- *Next file while capturing.* Next file is clicked every 250 ms while a
  capture of the edited folder runs; each step is timed until the next file's
  bytes are shown.
- *Cancel capture.* Capture revision, then Cancel capture in its place; timed
  until the page says the capture stopped, which must record nothing.
- *Open cold* is Open Project on a new profile: create, bind, first capture
  and the first drawer window. *Warm* is a restart that resumes the cycle from
  Home. The operating system's file cache is not purged.

**Corpora**, generated deterministically in a scratch root the course makes
and removes:

| Name | Files | Lines | Text bytes | Notes |
|---|---:|---:|---:|---|
| `small-files-1m-lines-v1` | 20,000 | 1,000,000 | 26,244,500 | 50 lines each, 100 files per directory |
| `tree-100k-files-v1` | 100,001 | 1,000,000 | 25,888,900 | 10 lines each in 100 × 10 directories, plus one 129 MiB file over the 128 MiB capture budget |
| `stress-round-v1` | on the first | | | 512 terminal notes and 2 desktop suggestions: one 514-item round, frozen, exported and read |
| `limit-250001-entries-v1` | 250,001 | 0 | 0 | one more entry than a capture keeps |

One file in a hundred is edited between captures. Percentiles are nearest
rank over the samples taken; a phase with one sample says so by its count.

**Targets.** p95 under 100 ms for note typing and navigation while a capture
runs, and progress plus cancellation for an operation over 250 ms (LIMITS).
The two timing targets are enforced in optimized macOS builds, whose
measurements below justify them with wide margin; elsewhere they are reported
with `enforced: false` until the stations' measurements do. A Debug build runs
a 2,000-file correctness pass and claims no measurement ([[BPR-0967]]'s
convention). Correctness fails the row everywhere: every key reaches the note,
every changed path and only those are listed, comparisons show every edited
line, the drawer reaches the last file, limits are named.

## Defects the measurement found, fixed at their source

The first full run, at `c71ce1258` (Release, the same Mac), found five, and the
fixes found a sixth. All are in `397908dec`.

1. **A capture stopped all reading.** The runtime ran store calls one at a
   time, and the capture was one call: Next file during a 100,000-file
   capture waited **9,836.8 ms**, until the capture ended. The capture also ran
   on the application's shared workbench effect thread, so every other
   workbench effect waited behind it. The desktop now gives the runtime a
   capture lane of its own (`review_cycle_runtime`, `review_cycle_desktop`).
   Reads and the tab record run beside a capture; the first call that could
   move the cycle on (a note, a decision, a round) waits for it, with
   everything queued after it, because the capture records against the cycle
   revision it was asked under. The terminal composes no lane and is
   unchanged.
2. **No progress, no cancel.** A 9–14 s capture showed nothing but a disabled
   button. The Updates line now says "Capturing: 18,432 files read" (then
   "checked again", then "recording"), and Capture revision becomes Cancel
   capture while one runs. A cancelled capture records nothing and says so.
3. **Files past 4,096 were unreachable.** The drawer listed 4,096 files and
   said "4096 files (list shortened)"; nothing reached the other 95,905. It now
   lists a window of 4,096 at a time, says "files 1–4,096 listed of 100,001
   captured entries", offers Previous files and Next files rows, and Next file
   and Previous file go on across a window's edge. Core and host state stay
   bounded to one window.
4. **A file over the budget read as merely unavailable.** The 129 MiB file
   showed "The captured bytes of … are not available" and no mark in the
   drawer. The comparison host now reports it as too large, the reading pane
   says it was not captured because it is larger than the 128 MiB one capture
   keeps, and the drawer marks it "(not captured)". A changed-only comparison
   lists it with an unknown change: its bytes cannot be said unchanged.
5. **The over-limit refusal did not name the limit.** It now reads "The folder
   holds more than 250,000 entries, the most one capture keeps; nothing was
   recorded. Narrow the scope." in the desktop and the terminal.
6. **Tab saves waited on the capture's write lock** in front of the next read
   (one navigation step took 273.7 ms once the lane was in). They are kept
   until the capture ends and saved before any reload reads them back.

Each fix has a contract that fails without it: `review_cycle_capture`
(`captures_beside_reads`: a read beside the capture, a write and the read
behind it waiting, progress, cancel) and `review_cycle_workbench`
(`lists_and_captures_a_large_project`: two windows, the edge, the too-large
file, reading beside a capture, the deferred tab save, cancel). With the
capture lane removed both fail ("the capture went to its own lane", "the
capture runs on its lane"); with the deferred save removed the workbench fails
("the tab moved during the capture is saved when it ends").

## Results — macOS Release at `397908dec`

Apple M4 Pro, 14 logical processors, 48 GiB, macOS 26.6.2, Apple clang,
Qt 6.11.2, offscreen 1920 × 1200 at DPR 1, `build/bpr1184-release` (the `dev`
preset with `CMAKE_BUILD_TYPE=Release`, arm64, minimum macOS 13.0,
`BPR_BUILD_TOUR_CLI=ON`), under the repository's network-deny runner. The row
`application_review_cycle_large_project` passed 1/1 in 181.83 s with the
targets enforced; the station was otherwise idle. The report is
[`BPR-1184-large-project/mac-release-report.json`](BPR-1184-large-project/mac-release-report.json),
SHA-256 `4a649b448541594f804432d6368be6406a4f608c2ae30757a851c6e8a947291c`,
`source.revision` `397908dec…` with 0 tracked changes.

**Desktop**, gesture to result inside the process; "busy" is the GUI thread's
longest event-loop turn during the gesture:

| Phase | 20,000 files: n / p50 / p95 / max ms | 100,001 files: n / p50 / p95 / max ms | Busy p95 / max ms (20k; 100k) |
|---|---:|---:|---:|
| Launch to ready (process start to window) | 1 / 354.4 | 1 / 351.7 | - |
| Open Project, cold (create, bind, capture, list) | 1 / 2,112.0 | 1 / 10,124.7 | 32.6; 38.5 |
| Resume after restart, warm | 1 / 97.7 | 1 / 162.8 | 36.0; 34.1 |
| Open a file from the drawer | 30 / 8.35 / 8.92 / 9.80 | 30 / 7.08 / 8.75 / 8.90 | 0.20; 0.18 |
| Next file | 30 / 6.08 / 7.47 / 13.8 | 30 / 4.78 / 6.32 / 7.18 | 0.20; 0.17 |
| Next file across a window's edge | 1 / 65.9 | 1 / 69.5 | 34.3; 36.7 |
| Drawer: next window of 4,096 | 4 / 56.1 / 59.9 / 59.9 | 24 / 56.9 / 62.9 / 63.4 | 33.5; 37.8 |
| Capture revision, unchanged | 1 / 1,476.6 | 1 / 5,982.3 | 0.38; 0.43 |
| Capture revision, 1% changed | 1 / 1,793.4 | 1 / 7,414.4 | - |
| **Key typed in a note while capturing** | 37 / 0.32 / **0.51** / 0.55 | 151 / 0.36 / **0.53** / 0.58 | - |
| **Next file while capturing** | 6 / 4.55 / **4.93** / 4.93 | 28 / 4.48 / **6.32** / 6.55 | - |
| Cancel capture, to stopped | 1 / 1.91 | 1 / 7.47 | - |
| All cycle changes on an edited file | 20 / 14.9 / 20.9 / 21.6 | 20 / 9.53 / 11.2 / 15.7 | 4.03; 1.01 |
| Quit, to released | 1 / 32.2 | 1 / 36.2 | - |

Memory (macOS physical footprint and its lifetime peak, resident in
parentheses): after the cold open 91.3 MiB (147.1) for 20,000 files and
149.3 MiB (205.1) for 100,001; after the whole course 96.6 MiB (152.7) and
165.8 MiB (221.9), which were also the lifetime peaks; after the warm open
80.5 MiB and 124.1 MiB. The drawer showed 5 and 25 windows; the last file
of each corpus was reached and opened.

**Terminal** (`bpr cycle`, one process per command, start to exit, with that
process's peak resident memory):

| Command | 20,000 files: n / p50 / p95 ms | Peak MiB | 100,001 files: n / p50 / p95 ms | Peak MiB |
|---|---:|---:|---:|---:|
| `open`, cold | 1 / 1,827.6 | 36.6 | 1 / 9,788.4 | 92.5 |
| `capture`, unchanged | 3 / 1,339.4 / 1,340.7 | 36.2 | 3 / 5,635.7 / 5,664.4 | 92.3 |
| `capture`, 1% changed | 1 / 1,432.2 | 36.4 | 1 / 6,457.5 | 92.2 |
| `compare --changed-only`, one page | 2 / 35.9 / 39.7 | 27.3 | 6 / 100.3 / 101.7 | 69.4 |
| `compare`, one full-source page | 20 / 25.9 / 26.6 | 24.6 | 20 / 57.8 / 60.7 | 53.7 |
| `compare --path`, one edited file | 20 / 40.2 / 41.7 | 27.8 | 20 / 100.7 / 103.3 | 69.7 |
| `show` | 10 / 22.7 / 23.3 | 16.7 | 10 / 23.7 / 34.3 | 16.7 |

The changed-only pages listed exactly the 200 and 1,000 edited files; the
129 MiB file was listed once more, with an unknown change.

**Stress round** (514 items on the 20,000-file corpus):

| Phase | n / p50 / p95 / max ms | Busy max ms |
|---|---:|---:|
| `bpr cycle note`, each of 512 | 512 / 30.3 / 31.6 / 48.1 | - |
| Resume with 512 notes | 1 / 3,034.0 | 35.0 |
| A suggestion, saved and listed | 2 / 3,026.4 / 3,059.9 / 3,059.9 | 18.0 |
| Share notes opens with 514 rows | 1 / 2,179.3 | **234.8** |
| Copy (freezes F1) | 1 / 4,268.3 | 36.5 |
| Save to File | 1 / 4,338.9 | 35.6 |
| `bpr cycle export`, each | 10 / 38.0 / 39.5 / 39.5 | - |
| `bpr cycle round read` | 1 / 25.4 | - |

The 70,080-byte document was identical by Copy, Save and `export`; a harness
read all 514 items once, with 2 suggestions, in one page. Footprint after the
freeze: 117.5 MiB.

**The limit.** Open Project on 250,001 entries is refused after 10,914.5 ms
in the desktop and 10,174.2 ms in the terminal (exit 5, `over_limit`), both
saying "The folder holds more than 250,000 entries, the most one capture
keeps; nothing was recorded. Narrow the scope."

**Targets.** Typing p95 0.51 and 0.53 ms, navigation p95 4.93 and 6.32 ms,
all under 100 ms and enforced. Captures over 250 ms showed progress ("Capturing:
18,432 files read") and Cancel capture, which recorded nothing.

## Mixed and negative findings

- **A large round is slow to change.** With 512 notes every save reloads every
  note, two store calls each, and each call opens its own SQLite connection: a
  suggestion takes about 3 s to appear, the restart 3 s, Copy and Save 4.3 s.
  The GUI stays free (busy at most 36.5 ms) except one turn of **234.8 ms**
  when Share notes builds 514 rows, over the 100 ms input target. At the
  admitted 4,096 items both would grow about eightfold. Not changed here: it
  needs a batched note read in the store and a lazily built sheet, and is
  recorded for a follow-up card (BPR-1184's report names it; allocation needs
  the main integrator).
- **Capture throughput is file-system bound.** 100,001 small files take 6.0 s
  unchanged and 7.4 s with 1% changed; the first capture with listing 10.1 s.
  Each read opens every directory of its path again without following links
  (BPR-1151's noted cost). Correctness and the containment rules come first;
  with progress, cancel and reading beside it, this is no longer a
  responsiveness defect, only a cost, and it is reported rather than changed.
- **The terminal pays a manifest decode per process.** At 100,001 files a
  `compare` page or path costs about 100 ms, of which about 23 ms is starting
  the process (`show`). A harness paging many files pays it each time.
- **Memory grows with the capture.** The desktop's footprint after a
  100,001-file course is 165.8 MiB against 96.6 MiB at 20,000; the terminal's
  capture peaks at 92.5 MiB. No budget is set from one machine.
- **Single samples.** Open, capture and cancel are one sample per run; their
  p50 is that sample. Earlier runs on the same Mac under other load gave
  cold opens of 2.0–3.4 s (20,000) and 10.1–13.3 s (100,001); only the run
  above is cited.

## Station requests

Each station runs the optimized tree at `397908dec` (or any later `main`
holding it), then its ordinary Debug tree, which runs the 2,000-file
correctness pass. Return the Release report byte for byte as
`docs/verification/BPR-1184-large-project/<platform>-release-report.json`
(the Python writes LF on every platform), with the host, load, and both
ctest times, and commit it with a note on [[BPR-1184]]. Nothing here is
enforced off macOS; the next round sets each platform's targets from its own
report. If the contained row stops before measuring, record its first
failing line, pin it the way [[BPR-1166]]'s row is pinned
(`scripts/verify_test_network_isolation.py`), rerun it uncontained and
record both. On Windows that is expected at the first `bpr cycle open`, as
the parity course's `bpr cycle list` stops contained (exit 7,
profile_unavailable, measured at `8fab05419`).

Linux (`bpr-main-linux`):

```sh
cd /home/mmarcin/BetterPullReview && git pull --rebase --autostash && git checkout --detach 397908dec && cmake --preset app-release -B /home/mmarcin/BetterPullReview/build/bpr1184-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DEPENDENCY_OFFLINE=ON && cmake --build /home/mmarcin/BetterPullReview/build/bpr1184-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox && ctest --test-dir /home/mmarcin/BetterPullReview/build/bpr1184-release -R "^application_review_cycle_large_project$" --output-on-failure; git checkout main
cmake --preset dev -B /home/mmarcin/BetterPullReview/build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build /home/mmarcin/BetterPullReview/build/dev-tour-cli && ctest --test-dir /home/mmarcin/BetterPullReview/build/dev-tour-cli -R "^application_review_cycle_large_project$|^review_cycle_capture$|^review_cycle_workbench$|^application_review_cycle_desktop_public_path$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^test_network_isolation_manifest$" --output-on-failure
```

Windows (`bpr-main-win`):

```powershell
cd C:\code\git\BetterPullReview; git pull --rebase --autostash; git checkout --detach 397908dec; cmake --preset windows-app-release -B C:\code\git\BetterPullReview\build\bpr1184-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DISTRIBUTION_BUILD=OFF -DBPR_DEPENDENCY_OFFLINE=ON; cmake --build C:\code\git\BetterPullReview\build\bpr1184-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox; ctest --test-dir C:\code\git\BetterPullReview\build\bpr1184-release -R "^application_review_cycle_large_project$" --output-on-failure; git checkout main
cmake --preset windows-dev -B C:\code\git\BetterPullReview\build\windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build C:\code\git\BetterPullReview\build\windows-dev-tour-cli --config Debug; ctest --test-dir C:\code\git\BetterPullReview\build\windows-dev-tour-cli -C Debug -R "^application_review_cycle_large_project$|^review_cycle_capture$|^review_cycle_workbench$|^application_review_cycle_desktop_public_path$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^test_network_isolation_manifest$" --output-on-failure
```

Expected: `REVIEW_CYCLE_LARGE_PROJECT_OK` and 1/1 for the Release row, with a
report whose `run.enforced` is false, `failures` empty and `source.revision`
`397908dec…`; every Debug-tree row passes. The Release report is at
`build/bpr1184-release/tests/end_to_end/bpr1184-large-project-report.json`.
The Release row's timeout is a provisional 2,400 s off macOS; report the
measured time, from which the next round sizes it.

## Human steering benefit

RC61-10 also asks for the eight-item, two-suggestion round compared with the
same review assembled by hand, counting preparation, steering turns,
reconstruction steps, omissions, corrections and rework. That needs a person
and a harness, so nothing here claims it. It is asked as one Accept or Reject
question, `BPR-1184-steering-benefit-v001`, on a frozen build, with the counts
to be written in the note; [[BPR-1184]] records the session and its outcome.
No token, cost or quality saving is claimed from batching.

**Outcome (2026-10-04).** The owner accepted
`BPR-1184-steering-benefit-v001` in session `db739d0e…` (event 5, decision
`e4ae81e9-860e-48c0-8f3a-1394fa0a0dfa`, handed back at event 6) with an
**empty note**. It is recorded as the owner's verdict that the BPR round
needed no more steering than the message typed by hand. It is not a
measurement: no turn, reconstruction, omission, correction or rework counts
were recorded, and no steering, token, cost or quality saving is claimed.

## Station returns

All three Release reports are in this folder. Linux ran at `397908dec` (1/1
in 133.38 s). Windows ran at `c906b0e7e`, which holds `397908dec` and only
course fixes, no `src/` change (1/1 in 1,629.14 s, uncontained as pinned).
Their figures, hosts and findings are on [[BPR-1184]]. In short:

- The paging, cancel and GUI-thread results hold on every platform. The
  longest GUI turn during a capture was at most 1.23 ms, and cancel recorded
  nothing.
- Windows is slower: navigation while capturing p95 about 22 ms; a 100,001-file
  cold open of about 45 s on both the desktop and terminal paths; a first read
  under Defender of about 5 ms a file, kept out of the timed phases; and one
  100 s compare outlier with the GUI free.
