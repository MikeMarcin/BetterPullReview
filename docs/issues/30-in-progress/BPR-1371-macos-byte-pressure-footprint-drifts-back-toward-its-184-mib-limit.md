---
schema: "implement-rfc.card.v1"
id: "BPR-1371"
title: "macOS: the byte-pressure course's footprint drifts back toward its 184 MiB limit while the live heap stays flat"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"owner, 2026-10-09: application_large_review_cache_bytes footprint median 178.8 MiB (155-190, one miss at 190.0) against 158.6 at BPR-1296's fix; Windows flaked too","reservation":"docs/issues/_reservations/BPR-1371--BPR-1371--a64e15ed93fa.json","related_cards":["BPR-1296","BPR-0967","BPR-1032","BPR-1327","BPR-1344","BPR-1360"]}
---

# BPR-1371: macOS byte-pressure footprint drifts back toward its 184 MiB limit

## Observation

On macOS `application_large_review_cache_bytes` (limit from [[BPR-1296]]:
footprint growth at most 184 MiB over `before_application`) is close to its
limit again. The live heap sampled at each selection is steady at 158.8 MiB,
but the footprint median is 178.8 MiB (range 155-190, one failure at 190.0)
against 158.6 at BPR-1296's fix. Windows also saw the row flake.

## Outcome

Find which change raised the footprint while the heap stayed flat. If the
cause is a real retention, fix it with a test. If it is measurement noise in
footprint against heap, propose a better-grounded assertion here and bring it
to the owner before the limit changes.

## Direct acceptance

- AC1: an alternated A/B on macOS, at least five runs each, of `main`,
  `3947245ca^` and BPR-1296's fix `fa3537d8c`, with the station's load
  recorded.
- AC2: the footprint rise is attributed to a change, with before/after
  measurement.
- AC3: a real retention is fixed with a test, or a better-grounded assertion
  is proposed here and taken to the owner before the limit changes.

## Findings so far (2026-10-09, stopped at the coordinator's wrap-up)

No code fix and no limit change were made. The owner's decision on the
assertion proposed below is still pending (AC3).

### A/B (AC1)

macOS 26.6.2, M4 Pro (14 cores, 48 GiB), Qt 6.11.2, Release tests-on trees
building only `bench_better_pull_review_large_review`. The trees ran
alternately in rotated order, six rounds each, through
`ctest -R ^application_large_review_cache_bytes$ -V`.

The station was not idle. Other agents were building, and the 1-minute load
before a run ranged from 8 to 208 (recorded per run). `vm_stat` showed the
compressor holding about 4.8 M pages with about 460 MiB free.

`3947245ca^` is `012f52ee4`. It already contains BPR-1344's code
(`c518a2e4e`), so `995e14c2c` (`c518a2e4e^`) was added as the true
pre-BPR-1344 tree.

| Tree | Pass | Footprint growth, lifetime peak MiB (min / median / max) | Live heap growth at selections MiB | 1 ms sampled heap peak growth MiB |
|---|---:|---|---:|---:|
| `af2b95e57` (main; source equal to `a2f02ce61`) | 4/6 | 163.6 / 174.2 / 185.7 | 158.8 | 165.4 |
| `012f52ee4` (`3947245ca^`) | 4/6 | 163.5 / 183.2 / 184.9 | 158.8 | 165.4 |
| `995e14c2c` (before BPR-1344) | 1/6 | 154.0 / 186.9 / 197.9 | 158.8 | 165.4 |
| `fa3537d8c` (BPR-1296's fix) | 5/6 | 157.4 / 172.9 / 185.1 | 154.5 | 161.0 |

Every miss was `sampled fixture footprint growth exceeded its platform
budget`. The four trees fall into two groups by live heap. Within each group
the heap is identical to 0.1 MiB, yet the footprint spreads by 30-44 MiB.

BPR-1296's own fix now misses in 1 of 6 runs and reaches 185.1 MiB. On
2026-10-06 the same code ran 20/20 with a highest value of 170.3. Runs with
load below 40 had a median footprint of 168.0 MiB (n=11); runs at 40 or above
had 184.4 (n=13). BPR-1344 has no effect: the three later trees have the same
heap, and their differences in footprint follow the load.

### What the footprint gap is

A diagnostic build of main (test file only, not committed) printed
`malloc_zone_statistics` and `TASK_VM_INFO` at every sample. It also ran
`footprint`/`vmmap` at four phases.

- At `before_loaded_close` the footprint is 196 MB. Of that, 162 MB is
  `MALLOC_SMALL` dirty and 28 MB is `MALLOC_LARGE`. Everything outside malloc
  totals about 5 MB, and that figure is the same at
  `application_constructed`.
- The gap is therefore pages that malloc holds but does not use. At the end of
  the course the zone holds 246-262 MiB against 159.4 MiB in use. The share of
  that free space the kernel still charges, rather than holding as
  `reusable`, decides the footprint. The gap above the heap was 13.7-36.4 MiB
  at the end and up to 46 MiB mid-course, and it moves inversely with
  `reusable` (gap plus reusable is about 45-52 MiB).
- Footprint jumps line up with compressor events. For example, between two
  selections `internal` fell from 71 to 21, `compressed` rose from 44 to 111,
  `reusable` fell from 35 to 0.4, and the footprint rose 17 MiB while the heap
  rose 3.4.
- `malloc_zone_pressure_relief(nullptr, 0)` released 0 bytes at every sample,
  so it cannot normalize the gap.
- Five more main runs of the diagnostic build gave 169.7 / 171.2 / 177.5
  (pass) and 187.7 / 187.9 (miss).

So the footprint drift comes mostly from allocator free-page accounting under
system memory pressure and scheduling. It is not retention.

### The real heap growth (AC2, partial)

Main holds 4.3 MiB more live heap than the fix, and the figure is
deterministic:

| Phase | `fa3537d8c` | main |
|---|---:|---:|
| `application_constructed` | 45.77 MiB | 48.57 MiB |
| `review_loaded` | 63.37 MiB | 68.86 MiB |
| Every selection | — | +4.2 MiB |

The transient above the steady heap is unchanged: sampled peak minus steady
is 6.5 MiB on the fix and 6.6 on main. Preparation added no churn.

A bisect on the constructed heap was interrupted after seven steps. It
attributes one step to `68f00eb15` ([[BPR-1089]], Overview label chips),
which is the only `src/` commit between good `3fa7527b8` (46.79 / 65.55) and
bad `09334cf71` (47.57 / 67.77). That step adds +0.78 MiB constructed and
+2.22 MiB loaded. The rest is spread out and unattributed:

| Span | Constructed | Loaded |
|---|---:|---:|
| `fa3537d8c` to `3fa7527b8` | +1.0 MiB | +2.2 MiB |
| `09334cf71` to main | +1.0 MiB | +1.1 MiB |

Whether BPR-1089's 2.2 MiB is a retention, such as a per-row pixmap or
bucket kept for 1,000 threads, has not been examined.

### Proposed assertion (for the owner; not applied)

1. Gate the live heap, which is deterministic here to about 1 MiB. Bound
   live-heap growth at every sample and the 1 ms sampled heap peak tightly,
   at about 6% above today's values:
   - growth at most 168 MiB against 158.8;
   - sampled peak growth at most 175 MiB against 165.4.

   That would have caught BPR-1296's 9.6 MiB heap step on its own.
2. Keep footprint and lifetime peak as a loose backstop at 224 MiB, the
   former BPR-0893 single bound. The highest value observed is 197.9, about
   10% below that. Report the zone's `size_allocated` and `TASK_VM_INFO`
   `reusable`/`compressed` beside it, so that a miss can be read as heap or as
   allocator.
3. Record the station's load and compressor state with each measurement.

Windows, which gates commit charge (`PrivateUsage`), is likely subject to the
same allocator effect but was not measured here.

### Next

- Finish the bisect of the remaining +2 MiB constructed / +3.3 MiB loaded.
- Check whether `68f00eb15`'s +2.2 MiB at review load is held per thread or
  per label bucket.
- Bring the assertion proposal to the owner. Change no limit until the owner
  decides.
