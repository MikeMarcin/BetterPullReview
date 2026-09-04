---
id: BPR-0555
title: The epoch fixture seed's budget is marginal on a busy machine
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The epoch fixture seed's budget is marginal on a busy machine

## Why

Four rows of the full `dev` matrix failed during RFC-BPR-0016's closeout on
2026-09-03 — `application_epoch_persistence_github-com`,
`application_epoch_maintenance_public_path`,
`application_appearance_gesture_contract`, and
`application_appearance_store_fallback_contract` — all with the same shape:
`EPOCH_FIXTURE_SEED_FAILED`, stage 2, status "Saving the observed revision…",
`PUBLIC_PATH_BUDGET:expired=1 budget_ms=60000 elapsed_ms=60023` to `68556`.

They are **not** that run's doing. Reproduced at `d6249844`, the head that
session started from, in a separate worktree built from scratch: both tests
checked fail identically there, with the same marker and the same budget
expiry. The machine at the time was running Spotlight reindexing at 127% of a
core (plausibly because that session created and deleted tens of gigabytes of
build roots), a browser at 133%, and a language server at 98%.

So the seed is not slow by construction — the same rows pass on a quiet
machine, and did earlier the same day — but its 60-second budget leaves no
room on a machine doing anything else, and the failure it produces names the
seed rather than the clock, so it reads as a defect in the epoch path rather
than as "this host was busy".

## Required final state

- A run that misses this budget says so as a budget, distinguishable at a
  glance from a seed that genuinely failed: the marker names the elapsed
  time against the bound and the stage it was in, rather than only
  `EPOCH_FIXTURE_SEED_FAILED`.
- The budget is set from a measurement rather than a round number, with the
  measured seed time on a quiet host and the headroom stated in the CMake
  comment beside it, as the device courses' bounds now are.
- If the seed itself can be made cheaper — it re-seeds a Git cache and a
  SQLite catalog per row — that is preferred over a larger budget, and the
  reason for whichever is chosen is recorded.

## Non-goals

- Changing what any of the four rows prove.
- Making the rows tolerant of a genuinely failed seed.

## Landed — 2026-09-04

At `60081ccc` plus the integrator's driver-script half.

**The marker.** `RUN_BUDGET_EXPIRED:elapsed_ms=… budget_ms=… over_ms=… stage=…
status="…"` is emitted whenever the run's budget expired, beside the verdict
rather than inside the block that only prints after a failure. Proven live by
forcing the phase budget to three seconds:
`RUN_BUDGET_EXPIRED:elapsed_ms=3018 budget_ms=3000 over_ms=18 stage=2`.

**The headline, which the marker alone did not fix.** The four driver scripts
dump the child's whole marker block on failure, so `RUN_BUDGET_EXPIRED` was
*in* the output but the first line still read "seed process failed", which is
the sentence a reader acts on and the reason these rows looked like an epoch
defect for a day. Each driver now lifts the budget line into the headline when
the child reported one, immediately under the failure line. Verified by the
same forced run: the message reads `seed process failed (1)` then
`RUN_BUDGET_EXPIRED:…` then the dump.

**The bound, from a measurement rather than a round number.** Three
consecutive seed phases at a 300-second budget measured **34.74 s, 33.34 s,
33.38 s** — on a host whose load averages read `5.11 8.03 7.98`, so a
*loaded* figure, stated as such because no quiet window was available and a
measurement taken under load is worth more than a number chosen to look
tidy. The budget moved 60 s → **120 s**: 4.8× the previously recorded quiet
figure, 3.5× the loaded measurement, and 1.75× the worst a real host has
taken (68.6 s, this run's own matrix). The dependent timeouts rose with it to
satisfy the harness's three-quarter rule.

One row was worse off than the card knew:
`application_appearance_store_fallback_contract` drove an epoch phase on the
40-second fixture budget, the thinnest margin in the file against a measured
33 to 35 seconds. It now uses the epoch budget.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A missed budget says so as a budget, in the headline | the phase budget forced to 3 s | `seed process failed (1)` followed by `RUN_BUDGET_EXPIRED:elapsed_ms=3018 budget_ms=3000 over_ms=18 stage=2` | this commit |
| The four rows and the device courses pass at the real bound | `ctest --preset dev -j 2` over the ten-row set | 10/10 | this commit |

**Left open.** The seed itself was not made cheaper: it seeds a Git object
cache and a SQLite catalog, which lives in layers this slice was not allowed
to touch and would change what the rows drive. That remains the better fix if
the budget is ever marginal again.

