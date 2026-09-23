---
schema: "implement-rfc.card.v1"
id: "BPR-0893"
title: "macOS: three large-review footprint budgets are red, two long-standing and one unstable"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-005 integrator, in the run's first full macOS sweep","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"AC1 The three rows pass on macOS, or each carries a re-derived budget with the measurement behind it","outcome":"pass","revision":"9904affb","evidence":{"kind":"command","detail":"application_large_review_cache_bytes, _loaded_close and _resources, with _cache_entries, pass in 18 of 18 runs (6 resources, 6 loaded_close, 3 cache_entries, 3 cache_bytes), the last set rebuilt at 9904affb. They pass on re-derived budgets. The sampled footprint budget is loosened from 160 to 224 MiB, which accepts Qt 6.11.2's retained HarfBuzz buffers (measured with 92c4e159^/92c4e159: large_file_ready 121.6-122.6 against 184.2-186.2 MiB; malloc_history attributes 70.4 MiB to shapeTextWithHarfbuzzNG); the highest observed peak is 202.9 MiB. Cleanup is bounded on the live heap at 96/32 MiB (observed 60.3-67.6 / 12.9-13.8). Before: 8 of 8 failed at the head. Negative controls: a 48 MiB block at large_file_ready fails the peak (233.2); 48 MiB at after_prune fails cleanup (live 115.6); 24 MiB at application_destroyed fails cleanup (live 37.7).","path":"docs/issues/60-done/BPR-0893-three-macos-large-review-footprint-budgets-are-red-and-one-is-unstable.md#evidence"}},{"criterion":"AC2 loaded_close's run-to-run variation is recorded, and the row either tolerates it or is made deterministic","outcome":"pass","revision":"9904affb","evidence":{"kind":"observation","detail":"Recorded: under Qt 6.10.3 its shutdown_drained footprint was 93.3/93.3 MiB at 92c4e159^ and 100.3/100.5 at 97b296bd, against the 96 MiB line, and dsp-009 logged a FAIL and a PASS at one tree. That variation is allocator state: under Qt 6.11.2 the footprint moves by up to 7 MiB between runs while the live heap repeats to 0.1 MiB. Made deterministic: cleanup now asserts the live heap, which is 65.6 / 13.7 MiB in all 6 loaded_close runs.","path":"docs/issues/60-done/BPR-0893-three-macos-large-review-footprint-budgets-are-red-and-one-is-unstable.md#why-the-three-rows-were-red-at-the-head-in-three-separate-causes"}},{"criterion":"AC3 The card states how long each row had been failing before this sweep","outcome":"pass","revision":"9904affb","evidence":{"kind":"observation","detail":"All three have failed deterministically at the peak since 92c4e159 (2026-09-21 16:05, the Qt 6.11.2 upgrade). Before that, under Qt 6.10.3, resources and loaded_close sat on the 96 MiB cleanup line, and cache_bytes exceeded 160 by 5-7%. They were thresholds on allocator state with no fixed onset, and their last passes remain dsp-009's (2026-09-13 00:41 / 00:36). dsp-009's figure discrepancy is reconciled: the first table paired peak samples with the cleanup limit, both in decimal MB.","path":"docs/issues/60-done/BPR-0893-three-macos-large-review-footprint-budgets-are-red-and-one-is-unstable.md#how-long-each-row-had-been-failing"}},{"criterion":"AC4 Linux and Windows are unaffected: these rows do not run there","outcome":"pass","revision":"9904affb","evidence":{"kind":"observation","detail":"The four rows are registered only inside if(APPLE) in tests/end_to_end/CMakeLists.txt. The new calls (malloc_zone_statistics, the growth lambda) are in sample_resources' Apple branch beside task_info; the fields and constants compile everywhere and are read only there. The all-platform ordinary benchmark, unavailable and approximate rows pass on macOS at 9904affb. Linux and Windows builds were not run here."}}]
---

# BPR-0893: macOS: three large-review footprint budgets are red, two long-standing and one unstable

## Defect

The run's first full macOS sweep was 378 of 381. All three failures are
macOS-only large-review rows, inside `if(APPLE)` in
`tests/end_to_end/CMakeLists.txt`, which is why Linux (367/367 green) and the
run's earlier focused passes never showed them:

- `application_large_review_cache_bytes` — "sampled Mac fixture footprint
  growth exceeded its provisional budget"
- `application_large_review_loaded_close` — "Mac cleanup footprint growth
  exceeded its provisional budget"
- `application_large_review_resources` — the same cleanup budget

Limits are `SampledFootprintGrowthLimitBytes` 160 MiB and
`CleanupFootprintGrowthLimitBytes` 96 MiB
(`tests/end_to_end/large_review_benchmark.cpp:115-116`); neither value has
changed recently.

## What was measured, and what it rules out

A baseline comparison was run at `85f47bde` — the commit this dispatch began
from — in a separate clean worktree, twice per row, against the current head:

| Row | at `85f47bde` | at the head | growth vs limit at head |
|---|---|---|---|
| `cache_bytes` | FAIL, FAIL | FAIL, FAIL | 163.4 MB / 167.8 MB, 2.6-3.2% over |
| `resources` | FAIL, FAIL | FAIL, FAIL | 131.0 MB / 100.7 MB, ~27-30% over |
| `loaded_close` | **PASS, PASS** | FAIL, FAIL | 144.7 MB / 100.7 MB, ~44% over |

So `cache_bytes` and `resources` are **long-standing** and predate this
dispatch. They are simply the first full macOS sweep making them visible.

`loaded_close` looked like a clean regression. Two further experiments say it
is not that simple:

- Reverting **only** `large_review_benchmark.cpp` to its pre-dispatch content
  (that is, undoing [[BPR-0856]]'s data-root change, the sole change to that
  file) and rebuilding: still FAIL, twice. **So BPR-0856 is not the cause.**
- Checking out the whole of `src/` at `85f47bde` into the same build
  directory: **FAIL then PASS**. The same source that passed twice in a clean
  worktree fails half the time here.

That last result is the important one: the row is **not deterministic**, and
the build directory or machine state influences it. A regression hypothesis
that rests on "it passed at the baseline" does not survive it.

## Scope

- Settle determinism first. Run `loaded_close` several times at a fixed head,
  clean build and incremental, and establish the distribution rather than a
  verdict. Everything else depends on whether this row measures the product or
  the machine.
- Then decide what the budgets are for. They are named *provisional*, and two
  have been exceeded for an unknown length of time, which means they are
  currently not gating anything — they are just red. Either they express a
  real ceiling, in which case find the growth, or they need re-derivation from
  measurement, in which case say so and record the basis.
- Establish how long `cache_bytes` and `resources` have been failing. The last
  known green full macOS state is not recorded anywhere in this run.
- Consider whether a footprint measurement belongs in a pass/fail row on one
  platform only. A number that drifts with the toolchain and the machine may
  be better reported than asserted.

## Acceptance

- The three rows pass on macOS, or each carries a re-derived budget with the
  measurement behind it.
- `loaded_close`'s run-to-run variation is recorded, and the row either
  tolerates it or is made deterministic.
- The card states how long each row had been failing before this sweep.
- Linux and Windows are unaffected: these rows do not run there.

## How long each row has been failing — established from the logs on disk, dsp-009

An `rfc-utility` on Fable inventoried every log that names the three rows:
32 logs (13 under `build/*.log` from 2026-09-12/13, 19 under `build/r2-logs/`
from 2026-09-17). No `Testing/Temporary/LastTest*.log` under any of the ~120
build directories retains a block for them (all overwritten), and
`docs/verification/**` names none. No log records the source-tree SHA; dates
are log mtimes except two logs with in-log timestamps. Growth figures below
are the failing `PhysicalFootprintBytes` sample minus the run's
`before_application` sample (`large_review_benchmark.cpp:199-205`); **the
harness never prints the growth number itself.**

| Row | Last recorded PASS | Earliest budget FAIL | 2026-09-17 record |
|---|---|---|---|
| `cache_bytes` | 2026-09-13 00:36 (`build/rfc0013-d2-byte-post-projection.log`, 333 s) | 2026-09-17 14:04 (`dsp5-mac-full-sweep.log`) | 9 of 9 FAIL, sampled growth 168.1–171.6 MiB vs 160 MiB |
| `resources` | 2026-09-13 00:41 (`build/rfc0013-d2-preserved-public-courses.log`) | 2026-09-17 14:04 | 7 of 7 FAIL, cleanup growth 96.7–102.0 MiB vs 96 MiB |
| `loaded_close` | 2026-09-17 18:27 (`dsp7-0901-full-sweep.log`, 13.55 s) | 2026-09-17 14:04 | flips: FAIL 14:04, 14:43 ×2, 14:54 ×2, 14:56, 16:33; PASS 14:29, 14:30, 14:56, 18:27 |

So the honest answer to "how long" is a **window, not a date**: `cache_bytes`
and `resources` were green on 2026-09-13 and red at the first 2026-09-17 run,
including at the `85f47bde` baseline (10:43 that day), and **no run of either
row is recorded between 2026-09-13 00:41 and 2026-09-17 10:43**. Whatever
moved them is in that four-day window. Changes to
`tests/end_to_end/large_review_benchmark.cpp` inside it: `284d1eac`
(2026-09-15 10:16), `e1424320` (2026-09-17 05:00), `b841dfb5` (2026-09-17
10:35, [[BPR-0856]], already excluded above), `fe75b3a4` (2026-09-17 10:42,
[[BPR-0858]]). The limits themselves are unchanged since `316390ce`
(2026-09-05) apart from `1681994a` (2026-09-12) re-indenting them and adding
`ByteDrainedFootprintGrowthLimitBytes`. Product changes in the window were not
inventoried; that is the next discriminating step and it is a bisect, not a
grep.

`cache_bytes` also flipped six times on the evening of 2026-09-12 (the
`bpr0635-*` logs), several for non-budget reasons, and `loaded_close` holds a
FAIL and a PASS **in one log** (`dsp5-lc-src-baseline.log`). On this evidence
`loaded_close` is nondeterministic independent of the tree, which the scope's
first item already suspected; the distribution still has to be taken on a
quiet host, and dsp-009's host was not quiet (a foreign test run held ~100% of
one core throughout).

**A discrepancy to reconcile before anyone quotes this card's numbers:** the
"growth vs limit at head" figures in the table above (163.4 / 167.8 MB,
131.0 / 100.7 MB, 144.7 / 100.7 MB) do not match the failing-sample arithmetic
from the same logs (`dsp5-current-*`: 177.9 MiB, 101.7 MiB, 100.7 MiB). Their
derivation is not recorded. Not reconciled here.

## Resolution — 2026-09-23, Mac integrator (Mac batch 2)

All measurements are on the Mac integrator (M4 Pro, 48 GiB, macOS 26.6.2,
Debug), through `ctest` in `build/dev` or a scratch worktree at a named commit.
Growth is the sample's value above the course's `before_application` sample.
A per-run table is under Evidence.

### Why the three rows were red at the head, in three separate causes

**1. The Qt 6.11.2 upgrade raised the peak by about 63 MiB, and that is why all
three are red now.** At the head all three rows fail at their *first* large
sample, `large_file_ready`, with 185.8–188.6 MiB against 160 MiB (8 of 8 runs),
before any cleanup phase. The discriminating pair is `92c4e159^` against
`92c4e159`, which upgraded Qt from 6.10.3 to 6.11.2 and whose only source
change is a Windows header. `large_file_ready` measures 121.6–122.6 MiB before
it and 184.2–186.2 MiB after (two runs of each row on each side, with Qt
confirmed from the report's own `QtVersion`). `malloc_history` on the same
course attributes it. The measurement's per-line layouts
(`plain_text_pane_measurement::advance`) hold 159,259 allocations for 17.0 MiB
under 6.10.3 and 159,176 for 73.5 MiB under 6.11.2. Under 6.11.2,
`QTextEngine::shapeTextWithHarfbuzzNG` alone holds 70.4 MiB in 119,340 live
allocations, three per line. Qt 6.11 (qtbase `8209078e0e`) keeps each
`QTextEngine`'s HarfBuzz buffer until the engine is destroyed, and a pane
keeps one layout per block. Every one of the 40,000 measured lines therefore
retains 1,856 bytes. No public Qt API releases it; [[BPR-1032]] owns what to
do about it. BPR-0940's move of the ordinary course into Read, the other
change today, is excluded: without it the peak is the same (184.5–188.1 MiB,
6 runs).

**2. The cleanup budget measured the allocator, not the product.** After
cleanup the process footprint stays near its peak, because freed pages stay
dirty in the allocator. At `application_destroyed` under 6.11.2, `footprint`
and `vmmap` show the default malloc zone with 14.6 MB live inside 106.7 MB
dirty (87% free). That includes 55.6 MB of *completely empty* small regions
and 22.1 MB of empty large regions. `malloc_zone_pressure_relief` before each
sample changes nothing (6 runs). The live heap (`malloc_zone_statistics`
`size_in_use`) tells the product's story, and it is exact from run to run:

| Course | peak live | after prune / delete | shutdown drained | destroyed |
|---|---|---|---|---|
| `resources` | 164.2 | 67.6 / 67.6 | 65.8 | 13.8 |
| `loaded_close` | 171.7 | — | 65.6 | 13.7 |
| `cache_entries` | 62.8 | — | 60.3 | 12.9 |
| `cache_bytes` | 231.0 | — | 65.5 | 12.9 |

(MiB of growth; identical to 0.1 MiB in every run of each course.) The
comparison is released: the live heap falls about 97 MiB from
`large_file_ready` to `after_prune`, and ends 13 MiB above start once the
application is destroyed. Only the footprint stays up, at 127.7–135.8 MiB for
`resources` and `loaded_close` and 175.9–177.9 MiB for `cache_bytes`.

**3. `loaded_close`'s flip was that allocator residue sitting on the line.**
Under Qt 6.10.3 its `shutdown_drained` footprint was 93.3 and 93.3 MiB at
`92c4e159^` and 100.3 and 100.5 MiB at `97b296bd`, against 96. `resources`'
`after_prune` was 96.1 and 95.6, then 102.0 and 98.0, at the same two
commits. dsp-009 also recorded a FAIL and a PASS in one log at one tree. The
value sat within 5% of the line either way. The movement is the allocator's:
under 6.11.2 the live heap at these phases repeats to 0.1 MiB, while the
footprint at the same phase moves by up to 7 MiB between runs.

### Decision: the peak budget is loosened, and cleanup measures the live heap

`tests/end_to_end/large_review_benchmark.cpp`:

- **The peak budget is loosened from 160 to 224 MiB, which accepts the Qt
  6.11.2 cost instead of fixing it.** The code cannot release the buffers
  through public API, so this is a re-derived budget rather than a fix. The
  highest observed peak is 202.9 MiB (`loaded_close`, `before_loaded_close`),
  and 224 MiB leaves 10% above it. It still bounds physical footprint at every
  sample. The comment beside it records the basis. [[BPR-1032]] asks for the
  budget to come back down if its fix lands.
- **Cleanup now bounds the live heap, not the footprint.** The limit is 96 MiB
  while the application is open (`after_prune`, `after_cache_delete`,
  `shutdown_drained`; observed 60.3–67.6) and 32 MiB once it is destroyed
  (observed 12.9–13.8). Retaining the large comparison would add about 97 MiB,
  which is over either limit. The allowance `ByteDrainedFootprintGrowthLimitBytes`,
  which let the byte course's drained footprint reach 160 MiB, is removed: on
  the live heap that course drains like the others.
- **Every `resource-sample` now prints `LiveHeapBytes`, `FootprintGrowthBytes`
  and `LiveHeapGrowthBytes`.** The harness no longer leaves the growth to be
  derived from two samples, which is where the figures reconciled below went
  wrong.

These rows still do not measure unsampled peaks or other platforms, and they
are not [[BPR-0967]]'s target-hardware budgets. That card's AC3 remains open;
this card supplies its macOS attribution (AC1), recorded there.

### How long each row had been failing

- **Since `92c4e159` (2026-09-21 16:05, the Qt 6.11.2 upgrade): all three,
  deterministically, at the peak.** No run at or after that commit passes.
- **Before it, under Qt 6.10.3: `resources` and `cache_bytes` marginally, and
  `loaded_close` intermittently.** `resources` and `loaded_close` sat on the
  96 MiB cleanup line (cause 3). dsp-009's `cache_bytes` sampled growth was
  168.1–171.6 MiB against 160. Their last passes remain dsp-009's
  (2026-09-13 00:41 and 00:36). The rows had no fixed onset: they were a
  threshold on allocator state, and they failed whenever that state drifted
  over it. A bisect of the 2026-09-13 to 2026-09-17 window would have hunted a
  flip rather than a change, so none was run.

### Reconciling dsp-009's discrepancy

The first table's "growth vs limit at head" pairs are a peak sample and the
cleanup limit, both in decimal MB. `100.7 MB` is 96 MiB and `167.8 MB` is
160 MiB. `131.0 MB` is `resources`' `large_file_ready` peak of 124.9 MiB, and
`144.7 MB` is `loaded_close`'s `before_loaded_close` peak of 138.0 MiB
(`build/r2-logs/dsp5-current-*-run1.log`). Neither peak is subject to the
cleanup limit. The failing samples in those logs were `after_prune` at
97.0 MiB and `shutdown_drained` at 96.1 MiB: about 1% over, not 30% or 44%.
dsp-009's failing-sample arithmetic was the correct reading.

## Evidence

Mac integrator, M4 Pro, 48 GiB, macOS 26.6.2 arm64, Debug. Growth figures are
MiB above `before_application`.

**Before, at the head** (`67d5be84`, Qt 6.11.2, unchanged benchmark):
`resources` 4 of 4 and `loaded_close` 4 of 4 fail at `large_file_ready` with
185.8–188.6 against 160. With BPR-0940's `enter_read` removed, 6 of 6 still
fail (184.5–188.1).

**The Qt pair** (scratch worktree, `92c4e159^` Qt 6.10.3 / `92c4e159` Qt
6.11.2, two runs of each row): `large_file_ready` 121.6–122.6 / 184.2–186.2.
At 6.10.3, `loaded_close` passes twice (`shutdown_drained` 93.3) and
`resources` fails twice at cleanup (`after_prune` 96.1, then
`after_cache_delete` 101.7). The 2026-09-17 baseline `97b296bd` reproduces
dsp-009: `large_file_ready` 122.8–123.7, and both rows fail at cleanup
(98.0–102.0).

**After, with this change** (`build/dev`, the change on `67d5be84`, then again
at `9904affb` after rebuilding on the other stations' landings): 18 of 18
runs pass.

| Row | runs | footprint peak | footprint at cleanup | live heap, open / destroyed |
|---|---|---|---|---|
| `resources` | 6 | 185.5–188.9 (`large_file_ready`) | 127.7–135.8 | 65.8–67.6 / 13.8 |
| `loaded_close` | 6 | 199.8–202.9 (`before_loaded_close`) | 127.7–129.0 | 65.6 / 13.7 |
| `cache_entries` | 3 | 82.1–83.7 | 75.8–83.0 | 60.3 / 12.9 |
| `cache_bytes` | 3 | 176.7–178.6 (`before_loaded_close`) | 175.9–177.9 | 65.5 / 12.9 |

`application_large_review_benchmark`, `application_unavailable_diff_public_path`,
`application_approximate_diff_public_path`,
`application_large_review_history_session_race` and
`large_review_replay_contract` pass at `9904affb`.
`application_large_review_history` takes no resource sample and was not rerun.

**Negative controls** (scratch build of the change on `1adf3c58`, Qt 6.11.2,
with an environment-gated hook that holds a block of touched memory from a
named sample on):

- no block: `resources` passes (peak 184.6, live 67.6 / 13.8);
- 48 MiB from `large_file_ready`, taken before its footprint is read:
  footprint 233.2 fails `sampled Mac fixture footprint growth exceeded its budget`;
- 48 MiB from `after_prune`: live heap 115.6 fails `Mac cleanup left live
  heap growth above its budget`, while the footprint (177.0) is under the
  peak budget;
- 24 MiB from `application_destroyed` in `loaded_close`: live heap 37.7 fails
  the same message against 32.

**Attribution method.** A temporary pause in `sample_resources`, removed
before the commit, held the process at a named phase. `malloc_history
-allBySize` under `MallocStackLogging=lite` was then diffed between Qt versions
and phases, and `footprint`, `vmmap -summary` and `heap -s` were read without
stack logging. `malloc_zone_statistics` before and after cleanup gave the
live-heap figures. Stack logging perturbs the allocator: its cleanup footprints
are lower, so only its live bytes and attributions are quoted.

**Static checks.** clang-format 23.1.1 passes the file. The project tidy script
reports findings only on untouched lines (62–76, 324, 460, 466, 836, 899, 1279,
1281).

**Linux and Windows are unaffected.** The four rows are registered only inside
`if(APPLE)`. Every new call (`malloc_zone_statistics`, the growth lambda) sits
in `sample_resources`' Apple branch, beside the existing `task_info`. The
struct fields and report constants compile everywhere and are read only there.
Those two builds were not run here.
