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
