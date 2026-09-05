---
schema: "implement-rfc.card.v1"
id: "BPR-0131"
title: "Bring the automated stage budget inside the harness timeout"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0119"]
---

# Bring the automated stage budget inside the harness timeout

The automated public-path run bounds each stage with an attempt budget, and on
exhaustion it calls `capture_automated_timeout_evidence`, which records the
stage, the epoch state, and the render evidence. That path is the only thing
that makes a hang diagnosable.

For at least one stage the budget outlasts the CTest timeout. BPR-0119 proved
it directly: forcing that stage's readiness condition to never hold produced a
**CTest timeout at 45 seconds** with no `PUBLIC_PATH_VERDICT`, no stage marker,
and no captured evidence — the process was killed from outside before its own
budget expired.

A hang that is killed externally reports nothing. That is precisely the
unattributable failure BPR-0119 spent its effort undoing, and it will recur for
any stage whose budget is larger than the harness allows.

## Why this is separate

BPR-0119 fixed one intermittent failure. This changes timeout behaviour for
*every* automated stage, so it needs its own verification: shortening a budget
can turn a slow-but-correct run on a loaded machine into a failure.

## Required direct evidence

- Every stage's worst-case budget is shown to expire before the CTest timeout
  for that test, with the arithmetic stated rather than assumed.
- A deliberately stalled stage produces `PUBLIC_PATH_VERDICT`, the correct
  stage number, and the captured evidence — not an external kill.
- The budgets still tolerate a slow run: the suite passes repeatedly under
  load, and the margin between the largest budget and the timeout is stated.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

The per-stage attempt budget is replaced by one wall-clock budget for the whole
run. Ticks were never the right unit: a tick budget is comparable to a CTest
`TIMEOUT` only if every tick costs what it costs on an idle machine, and under
load the event loop stretches exactly when the harness clock does not. The old
6,000-tick budget was nominally 60 seconds and outlasted four of the timeouts
containing it.

One budget rather than one per stage, because what has to be provably smaller
than the timeout is the process's own lifetime. Per-stage budgets sum, and the
sum is what the harness would have killed.

The budget arrives from the harness through `--automated-run-budget-ms`, so it
sits beside the timeout it has to fit inside. A value that does not parse is
refused rather than replaced by the default: a budget silently reverted is one
the harness believes it set, and the first sign of that would be a timeout
reporting nothing — the failure this card exists to end.

`bpr_bound_automated_run` is now the only way these tests get a `TIMEOUT`. It
refuses any pair where the worst case the application permits itself exceeds
three quarters of the harness allowance, and prints the arithmetic at configure
time. A timeout cannot drift away from its budget without the configure step
saying so.

`application_automated_run_budget` is new. `--automated-stall-probe` holds the
`open` stage forever — the shape of a hang, where nothing crashes and a
readiness condition simply never becomes true.

## Evidence

Full arithmetic table, observed runtimes, and margins:
`docs/verification/RFC-BPR-0003.md`, "Automated-run bounds".

The stalled stage reports itself and exits in 6.39 s inside a 30 s timeout:

```
PUBLIC_PATH_STAGE:2
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0 epoch_projection_failed=0 epoch_handoff_failed=0
PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0 render=1
PUBLIC_PATH_BUDGET:expired=1 budget_ms=5000 elapsed_ms=5928 stage_ticks=505
```

`stage_ticks` separates two failures that otherwise read the same: 505 ticks in
5.9 seconds is a run that was waiting, and a handful in the same interval is a
run whose event loop was blocked.

Four breaks, each built successfully before being run:

| Break | Result |
|---|---|
| Budget guard disabled | CTest killed the process at 30.04 s with no markers at all |
| Expiry recorded but never flagged | `expired=0`, regular expression not found |
| Stall moved from `open` to `connect` | `PUBLIC_PATH_STAGE:1`, regular expression not found |
| Fixture budget raised to 50 s under a 60 s timeout | Configure failed: 52,000 ms exceeds the 45,000 ms share |

Smallest budget-to-observed ratio is 2.4x; the margin left to CTest is never
less than 5 seconds nor less than a quarter of the timeout. The end-to-end
subset passes idle and again under six busy CPU workers.
