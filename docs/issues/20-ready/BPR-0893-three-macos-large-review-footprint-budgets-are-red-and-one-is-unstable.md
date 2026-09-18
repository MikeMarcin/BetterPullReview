---
schema: "implement-rfc.card.v1"
id: "BPR-0893"
title: "macOS: three large-review footprint budgets are red, two long-standing and one unstable"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-005 integrator, in the run's first full macOS sweep","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
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
