---
schema: "implement-rfc.card.v1"
id: "BPR-1297"
title: "Debug presentation of a 20,000-line file under cache pressure takes 135–372 s per selection"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"BPR-1265 Windows Debug runs, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1297--BPR-1297--6e67e2e347b2.json","related_cards":["BPR-1265","BPR-1264"]}
---

# BPR-1297: Debug presentation of a 20,000-line file under cache pressure takes 135–372 s per selection

Found by bpr-main-win while fixing [[BPR-1265]]: Windows 11 Home 10.0.26200,
MSVC 14.51, Qt 6.11.2, offscreen, `build/windows-dev` Debug, at `3036a08af`
and later. It has not been measured on macOS or Linux. This is a low-priority
investigation and gates nothing. Since [[BPR-1265]],
`application_large_review_cache_bytes` runs only in optimized builds, and a
Debug run reports Skipped.

## Observation

Before [[BPR-1265]]'s job-assignment fix, the course's refreshes failed
early. Once they no longer did, three Debug runs of
`application_large_review_cache_bytes` each reached the
`cache_byte_selection` phases. There the course clock (2,520 s, the Windows
Debug scale of 3) expired at selection 6, 7 or 11.

| Phase | Debug, per phase | Note |
|---|---|---|
| `byte_epoch_refresh_{2..32}` | 25–48 s each | Grows through the course |
| `byte_first_head_selection` | 3.9 s | |
| `cache_byte_selection_{1..}` | 135–372 s each | More than ten per course |

One selection, `cache_byte_selection_2`, in detail:

- 135.7 s and 3,315 event-loop pulses;
- only 528 pulses with diff work, so 2,787 had none;
- P95 pulse interval 61.3 ms, the largest 588 ms;
- at most one diff job at a time.

So for most of the phase the GUI thread is busy presenting the 20,000-line
file, under the byte course's 30 MB cache pressure, not computing its diff.
In an optimized build the whole course takes 280–316 s.

The one recorded Debug pass of this course, at `f61340e17`, took 1,626 s in
all. That was before [[BPR-1264]]'s bounded fill (`ac0d9d1d4`) and the later
`diff_workbench` changes (`5feb7f3b3`, `201c96f21`, `b109295f4`). A clean
comparison against the bounded fill was not possible at current `main`,
because reversing `ac0d9d1d4` conflicts with those later changes.

## Outcome

Profile the Debug presentation of one cache-pressure selection. Candidates
include the bounded fill's batches, `QTextDocument` layout, classification
and syntax colouring, and MSVC's checked iterators. Then decide whether this
is a Debug-only cost, or a regression the optimized build hides and that is
worth fixing.

## Direct acceptance

- The card records where a Debug selection's time goes, measured.
- It records either a fix, with a before-and-after measurement, or the
  owner's decision that the cost is Debug-only and stays.

## Cause and fix (bpr-main-win, 2026-10-06)

**It is not presentation.** The time went to the course's own measurement.
`live_heap_sampler` (`tests/end_to_end/large_review_observers.hpp`) counts the
live heap with `HeapSummary(GetProcessHeap())` on Windows, which holds the
heap's lock, and then waited a fixed 1 ms. Its sample rate shows what each
sample cost:

| Build | Samples per second | Per sample | Heap lock held |
|---|---:|---:|---:|
| Debug (`cache_bytes`) | 66 | about 14 ms | about 93% of the time |
| Optimized (resource courses) | 88–155 | about 5–10 ms | about 85–90% |

While the lock was held, every allocation on the GUI thread waited. That is
also [[BPR-0967]]'s "the 1 ms heap sampler perturbs the GUI thread it
measures".

**A/B in the Windows Debug row** (`build/windows-dev`, MSVC 14.51, with the
Debug skip bypassed in a local build that was not committed):

| Sampler | Each `cache_byte_selection` | Each refresh | Course |
|---|---|---|---|
| 1 ms, as before | 135–372 s | 25–48 s | never finished in 2,520 s |
| off | 10.5–19.1 s | about 7 s | 22 selections and the final revisit in about 522 s |
| bounded (this fix) | 13.5–14.2 s | 6.0–8.6 s | **passes in 569.0 s** |

The bounded run took 13.6 samples per second; its largest interval was
164 ms, and it still caught the 182 MB peak.

**Fix** (test harness only). After each sample, the sampler waits the larger
of 1 ms and four times that sample's cost, so it holds the lock at most about
a fifth of the time. A cheap sample, as on macOS or Linux or in a small heap,
still repeats every millisecond. `MaxSampleIntervalMs` keeps reporting the
interval actually reached. The timing control's 32 MiB, 50 ms allocation is
made in a small heap, and it is still caught.

**The optimized build, with the fix** (`build/bpr0967-release`, MSVC `/O2`):

- The 18 large-review rows pass 18 of 18 with `-j1` in 506.0 s. All 14
  reports have `BudgetsEnforced` true, an empty `KnownInputMisses` and no
  limit miss.
- `cache_bytes` took 210.7 s, against 280–316 s before.
- The ungated sampler courses' load gap dropped:

  | Course | `paged_review_and_git_load` gap, before → after |
  |---|---|
  | `cache_bytes` | 159 → 70 ms |
  | `cache_entries` | 180 → 126 ms |
  | `resources` | 195 → 58 ms |
  | `loaded_close` | 182 → 144 ms |

- One gated value, the history course's 92.0 ms, was noise: rerun alone it
  measured 53.5 ms, and that course runs no sampler (0 samples).
- Debug: `cache_entries`, `loaded_close`, `resources` and `timing_control`
  pass; `cache_bytes` reports Skipped, as [[BPR-1265]] chose.
- clang-format 22.1.3 is clean.

The sampler is shared test code. The coordinator runs the optimized rows on
macOS and asks Linux.
