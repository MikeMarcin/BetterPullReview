# BPR-0967 Windows and Linux large-review budgets — 2026-10-01

Optimized station measurements, the limits they justify, and the decisions
for the Debug rows. The station summaries are in
[`BPR-0967-station/`](BPR-0967-station/). The committed Windows files have LF
line endings; the station's CRLF originals hashed `14452A36…` (Release) and
`E97685BD…` (Debug), and their content is otherwise identical. The Linux files
are byte-for-byte copies.

## Stations

| | Windows (bpr-main-win) | Linux (bpr-main-linux) |
|---|---|---|
| Candidate | `682ea1392` (`cef11b982` plus the `windows.h`-before-`psapi.h` build fix) | `e257b236a` (BPR-1262, which made the GCC 15 tests-on Release graph build) |
| Host | i9-10900KF, 20 logical, 128 GiB, Windows 11 25H2 10.0.26200 | i9-10900KF, 20 logical, WSL2 |
| Toolchain | MSVC 19.51.36257, `/O2 /Ob2 /DNDEBUG` (windows-app-release, tests on) | GCC 15.2, `-O3 -DNDEBUG` (app-release, tests on) |
| Qt, style | 6.11.2, Fusion, DPR 1, offscreen | 6.11.2, Windows style override, offscreen |
| Release | 16/16 in 564.92 s, load 10% → 3% | 16/16 in 93.37 s |
| Debug | 14/16 in 1030.49 s | 16/16 in 179.27 s |

Each platform ran the same 16 large-review rows in an optimized tree and in
its ordinary Debug tree. These are fixture compositions, not packaged
launches, compositor presentation or physical input.

## Memory counters

The Windows report pairs private commit with `PeakPagefileUsage`. Linux pairs
`VmRSS` with `VmHWM`; its resident figure includes file-backed pages, so Linux
growth is not comparable with the Mac physical footprint. Live heap is
`HeapSummary` of the process heap on Windows and `mallinfo2` on Linux.

The timing control's held-allocation check passes on both. Its floor is the
32 MiB allocation itself, so a correct sampler clears it by only the
allocator's bookkeeping: Windows +4,637 bytes in Release, Linux +4,096 bytes,
the Mac +128 bytes. The margin is not a weakness of the sampler. The Windows
sampler, though, ticks at the 15.6 ms system timer (16.2 ms largest interval
in the control, five samples in a 50 ms hold), and `HeapSummary` costs up to
35.5 ms under the byte course's heap: its largest interval there is 46.0 ms.
Windows sampled heap peaks are lower bounds with that resolution.

## Limits

Rule: a Mac limit is kept when the platform's highest optimized observation is
at most two thirds of it; otherwise the platform's limit is that observation
plus half, rounded up. For memory the margin is a tenth: kept when the
observation is at most 1/1.1 of the limit, otherwise the observation plus a
tenth, rounded up to 8 MiB (BPR-0893's rule). One run per platform; the
confirmation run tests the margins.

Changed limits:

| Limit | Mac | Windows | Linux |
|---|---:|---:|---:|
| Footprint growth, resources / loaded close / cache entries | 160 MiB | 160 (observed 120.95) | **168** (observed 147.04) |
| Footprint growth, byte pressure | 184 MiB | **208** (observed 185.43, the miss the Release report named) | **216** (observed 196.01) |
| Active close to quiescence | 100 ms | **150** (observed 98.85) | 100 (observed 11.63) |

The sampled live-heap peak reuses each course's footprint allowance, as on the
Mac. Every other limit holds with the margin the rule requires and is kept.
The closest are Windows: accepted metadata 130.8 of 250 ms, main to Home 615 of
1,000, revisit 514 of 1,000, launch to Home 558 of 1,000, cleanup live heap
71.96 of 84 MiB, and sampled heap peak 164.15 of 184 MiB in the byte course.

## The 100 ms input target

The target is unchanged. Gated phases measured at or over 90 ms on a platform
are named in that platform's `PlatformKnownInputMisses`. They are reported in
`Extended.KnownInputMisses`, and in `InputBudgetFailures` when they go over,
but they are not enforced. Every other phase gates.

- **Windows:** `large_file_selection` (94.1–130.5 ms),
  `large_source_revisit_render` (105.8–106.5), `shutdown` (93.4–105.8),
  `files_drawer_open` (98.2 for the whole phase), `active_window_close` (93.2),
  `paged_review_and_git_load` (98.2).
- **Linux:** `large_file_selection` (97.2–100.2),
  `large_source_revisit_render` (92.2).

[[BPR-1264]] owns attributing and repairing them. The resource and history
courses do not gate input; their larger Windows gaps (`paged_review_and_git_load`
208–315 ms while the heap sampler runs) are recorded, not limits.

## Debug rows

A Debug build off macOS enforces no numeric limit. None was justified by a
Debug measurement, the same principle as [[BPR-1247]]. macOS keeps its
BPR-1247 behavior. `large_review_launch.py` now takes `--enforce` from CMake:
1 on macOS and in optimized builds elsewhere.

The two Windows Debug failures have different causes:

- **`application_large_review_resources`, `public prune timed out` at
  105.94 s.** This is the course clock: 90 s for the whole course, and the
  Debug loaded-close course already took 93.83 s. Windows Debug now doubles
  every course clock. A clock is a run budget, not a performance limit.
- **`application_large_review_cache_bytes`, `byte-pressure stored head refresh
  timed out` at 323.46 s.** Its clock was 840 s, so the clock did not end it.
  The byte course's wait also stops on a failed, unavailable or cancelled epoch.
  It is not scaled away; it stays a named Debug failure, and [[BPR-1265]] owns
  it. Each report now records `Extended.WaitExpiry`: the wait, its clock,
  elapsed time, epoch status and whether the clock expired.

## Row timeouts

Windows timeouts keep the doubled clock plus a 30 s reserve within 0.75 of the
timeout, and stay at least three times each measured Debug completion:

- 300 s for the 90 s-clock rows (largest Debug completion 93.83 s);
- 540 s for cache entries (61.63 s);
- 1,500 s for history (187.47 s);
- 2,280 s for byte pressure (Release 280.70 s; no Debug completion yet).

The Debug resources completion is not yet measured. `large_review_replay_contract`
reached its 30 s bound on Windows alone and in the suite, which is a lower bound
only. It gets 120 s there until the confirmation run measures it; macOS
Release runs it in 2.03 s.

macOS and Linux keep their timeouts. Every Linux Debug row finishes in under
a third of its timeout: byte pressure 65.63 s of 900, history 46.68 of 600, and
the rest under 9 s.

## Mac verification

Mac Release (M4 Pro, Qt 6.11.2), at the code committed as `f61340e17`:

- The 16 large-review rows, `architecture_boundaries` and the replay contract
  pass. Every Mac limit is enforced with no miss and no input failure; the run
  log's SHA-256 is `fc140822…1956`.
- Two temporary controls at a 60 ms target, with the source restored
  byte-for-byte afterwards:
  - With `large_file_selection` and `large_source_revisit_render` named, the
    responsiveness course passes and reports both.
  - With only `large_file_selection` named, it fails, with
    `large_source_revisit_render_input_gap` enforced.

`large_review_summary.py` now keeps failed and timed-out rows, whose CTest
status can wrap across lines. A synthetic log with a wrapped failure, a timeout
and a pass is parsed correctly. Rows are keyed by log file.

## Confirmation at `f61340e17`

Both stations reran the 16 rows plus `large_review_replay_contract` at the
enforcing commit (summaries `windows-confirm-*`, `linux-confirm-*`).

| | Windows | Linux |
|---|---|---|
| Release, enforced | 17/17 in 444.37 s; no limit failed | 17/17 in 89.30 s |
| Debug, reported only | 17/17 in 2,358.98 s | 17/17 in 189.24 s |
| Replay contract | Release 2.56 s, Debug 33.23 s | Release 1.68 s, Debug 16.32 s |

On Windows Debug the resources course now completes (110.80 s). The byte course
passed in 1,626.01 s with no wait expiry. That is the second sample for
[[BPR-1265]], whose 323 s ending is not reproduced. Every Release report has
`BudgetsEnforced` true and no limit miss. The launch reports enforce.

## Gate every unsampled course — `9aa3df3bb`

The Linux confirmation reported `approximate_file_selection` at 117.4 ms, and
the row passed: the approximate course, like the unavailable, first-tab,
ordinary benchmark and history courses, recorded input misses without failing.
In an optimized build every course now gates the 100 ms target on all its
phases. The exception is the four resource courses, whose 1 ms heap sampler
perturbs the GUI thread it measures (Windows `paged_review_and_git_load` gaps
of 208–315 ms with it, 39–60 ms without).

The newly gated phases at or over 90 ms on a platform are named, and
[[BPR-1264]] owns them:

- `approximate_file_selection` on macOS (96.3–111.1 ms), Windows (153.7–179.9)
  and Linux (117.4–127.6);
- `first_tab_file` on Windows (122.4–154.1) and Linux (99.8).

The target is unchanged.

The confirmation also showed the doubled Windows Debug byte-course clock was
nearly used up: 1,626 s of 1,680. Windows Debug now triples every course clock,
and the Windows timeouts follow the same invariant:

| Rows | Timeout | Measured Debug completion | Ratio |
|---|---:|---:|---:|
| 90 s-clock rows | 420 s | largest: resources 110.80 s | 3.8× |
| Cache entries | 780 s | 61.07 s | |
| History | 2,220 s | 187.44 s | |
| Byte pressure | 3,420 s | 1,626.01 s | 2.1× |

The byte-pressure timeout is below three times its completion, which would be
81 minutes; the 2,520 s clock itself is 1.55 times the completion. These
replace the timeouts in the "Row timeouts" section above.

The Linux replay contract gets 60 s, 3.7 times its 16.32 s Debug completion.
Its 30 s bound was only 1.8 times that.

**Mac Release at `9aa3df3bb`:** the 16 large-review rows, `architecture_boundaries`
and the replay contract pass in 153.72 s (log SHA-256 `e1d5cf1a…b297`). With
macOS's name removed, the approximate course fails on its own 111.1 ms
selection gap with "fixture input target missed outside the platform's named
phases". That failure was the natural control for the new gate; the source was
restored byte-for-byte.

## Linux named phases removed — [[BPR-1264]], 2026-10-05

The four Linux phases were one cause: a publication replaced both panes' text
in one GUI turn. Filling the documents over bounded turns brings them to
23.4–36.6 ms on the same station, and Linux's `PlatformKnownInputMisses` is
now empty. With every phase gated, the 18 large-review rows pass in the
optimized tree. The Windows list is unchanged until its own optimized run;
[[BPR-1264]] has the numbers and the owed run.

## macOS named phase removed — [[BPR-1264]], 2026-10-05

The same change on the M4 Pro (`ac0d9d1d4`, Apple clang 21 `-O3`, Qt 6.11.2),
largest gap in ms over three runs each, before (the previous
`diff_workbench`) and after:

| Phase | Before | After |
|---|---|---|
| `approximate_file_selection` | 93.1–95.3 | 18.6–18.9 |
| `large_file_selection` | 62.3–64.7 | 18.6–18.9 |
| `large_source_revisit_render` | 64.7–68.2 | 12.2–14.0 |
| `first_tab_file` | 64.4–65.7 | 9.8–12.8 |

macOS's `PlatformKnownInputMisses` is now empty. The 18 large-review rows pass
in each of the three runs with every phase gated, `BudgetsEnforced` true and no
input or limit failure. The largest gap outside the resource courses is
36.4–38.0 ms (`first_tab_base`). The BPR-0967 runs had measured
`approximate_file_selection` at 96.3–111.1.

## Windows named phases removed — [[BPR-1264]], 2026-10-05

The same optimized tree as the station runs above (`build/bpr0967-release`,
MSVC 19.51 `/O2`, i9-10900KF). It was at `d85ce5879`, which holds the
bounded fill (`ac0d9d1d4`). Each column below is three serial runs of the 18
large-review rows. Values are the largest gap in ms over the gated courses;
`files_drawer_open` is the whole phase.

| Phase | Without the bounded fill | Main, before the list change | Main, every phase gated |
|---|---|---|---|
| `large_file_selection` | 122.1–131.6 | 29.9–32.1 | 27.8–32.1 |
| `large_source_revisit_render` | 94.5–103.5 | 22.0–24.2 | 18.6–22.7 |
| `approximate_file_selection` | 146.0–191.3 | 32.4–39.1 | 34.8–36.6 |
| `first_tab_file` | 120.7–127.3 | 25.1–29.7 | 28.4–29.6 |
| `shutdown` | 23.0–23.7 | 21.6–24.1 | 22.2–28.0 |
| `files_drawer_open` (whole phase) | 72.7–81.0 | 69.2–70.1 | 67.6–68.7 |
| `active_window_close` | 17.5–20.1 | 15.6–18.7 | 17.4–28.2 |
| `paged_review_and_git_load` | 50.9–58.4 | 53.1–57.4 | 53.2–56.2 |

- **Without the bounded fill,** with the list emptied, five rows failed in
  each run on the first four phases.
- **With every phase gated,** the 18 rows pass in each run (522.9–578.9 s).
  Every report has `BudgetsEnforced` true and nothing in `KnownInputMisses`.
  The largest gated gap is 53.2–56.2 ms.
- **The other four phases** no longer reproduce, with or without the change.
  [[BPR-1264]] has the attribution.

Windows' `PlatformKnownInputMisses` is now empty, as Linux's and macOS's are.
The resource courses stay ungated.
