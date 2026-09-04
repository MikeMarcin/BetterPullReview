---
id: BPR-0555
title: The epoch fixture seed's budget is marginal on a busy machine
status: Ready
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
