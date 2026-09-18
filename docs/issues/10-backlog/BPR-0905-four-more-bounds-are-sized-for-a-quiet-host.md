---
schema: "implement-rfc.card.v1"
id: "BPR-0905"
title: "Four more bounds are sized for a quiet host, one has already expired, and nothing says what margin a bound needs"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-007, from BPR-0901's fix and bpr-linux's BPR-0880 full-suite runs","found_at":"22023667","run":"run-20260916-stations","dispatch":"dsp-007"}
---

# BPR-0905: Four more bounds are sized for a quiet host, one has already expired

## Defect

[[BPR-0901]] turned out not to be an epoch defect at all. Two rows drove
`--epoch-fixture-phase` on the 40 s fixture *render* budget instead of the
120 s epoch-phase budget, and expired by tens of milliseconds under load —
nine recorded expiries of one row across four dispatches, every one within
53 ms of the bound. Fixing it meant looking at the neighbours, and the
neighbours are not well either.

**Four bounds with a thin margin, found while fixing that one:**

| Row | Bound | Measured | Margin | Already expired? |
|---|---|---|---|---|
| `application_saved_server_contract` | 40 s per phase, 4 phases | ~23.7 s each | **1.7x** | **Yes** — `build/r2-logs/dsp2-0818-siblings-a445866d.log:162` |
| `application_defect_composition_contract` | 40 s per phase, 3 phases | ~18.0 s each | 2.2x | Not seen |
| `application_review_ledger_composition_contract` | 40 s per phase, 2 phases | ~18.4 s each | 2.2x | Not seen |
| `sqlite_epoch_catalog_behavior_contract` | 180 s row timeout | 122.65 s alone | **1.47x** | **Yes** — timed out at 180.03 s in both of `bpr-linux`'s full-suite runs |

The first three are the same shape as BPR-0901's two: rows that drive
`--epoch-fixture-phase` on a budget measured for something cheaper.
`application_saved_server_contract` is the same row shape with the same
margin and has already failed that way once. The fourth is a different
mechanism — a CTest timeout rather than a run budget — reported by `bpr-linux`,
which established it is not a regression by running the row in isolation on
the same tree.

Rows that pass `--epoch-fixture-phase single` are a 12-13 s render and are
correctly budgeted at 40 s. Those are not in scope.

## The class is the finding, not the four rows

Fixing four bounds is an afternoon. The reason this is carded is that the same
shape has now appeared in three separate investigations in one run, in three
different mechanisms, and each was read at the time as a defect in the thing
being measured:

- [[BPR-0901]] — a run budget. Read as an epoch fixture defect for four
  dispatches. It was a clock.
- This card's fourth row — a CTest timeout. Read as a possible BPR-0880
  regression until someone ran it alone.
- [[BPR-0893]] — three macOS footprint budgets, explicitly named *provisional*,
  two exceeded for an unknown length of time, one that passes twice in a clean
  worktree and fails half the time in `build/r2-dev`.

**A bound with a thin margin does not fail honestly.** It fails as though its
subject were broken, and it costs whoever reads it a wrong diagnosis. That is
the actual cost, and it is much larger than the bound being wrong.

There is a related observation worth keeping: one host recorded
`application_workbench_composition_contract` expiring at `elapsed_ms=40020`
while, in the same run, a row on the 120 s epoch budget also expired
(`build/r2-logs/dsp4-linux-sweep-mac-classify.log:262`). A host in that state
survives no bound in this file, and a rule has to say what it does about that
rather than pretend it cannot happen.

## Scope

- Fix the four rows, and for each say what the new bound is **derived from** —
  the isolated measurement, the worst observed contended run, or a stated
  multiple. A number with no derivation is how the present ones got here.
- Then decide whether this tree wants a stated rule for run budgets and row
  timeouts, and write it where a person adding a row will meet it. Three
  candidates, not equivalent:
  1. a fixed multiple of the measured quiet time;
  2. a bound derived from the slowest contended run observed across the three
     stations;
  3. making an expiry **report itself as an expiry** in the failure headline,
     so the diagnosis is right even when the number is wrong.
  The third is the cheapest, helps every row including ones nobody has
  measured, and is what [[BPR-0901]] already did for its two drivers —
  generalising it is the highest-value part of this card.
- Survey the remaining rows whose `TIMEOUT` or `BUDGET_MS` sits close to their
  measured time. This is a grep and a table, not an experiment, and it is the
  only way to learn whether there are four of these or forty.
- **Do not raise every bound as a reflex.** A bound that never fails measures
  nothing, and two of the three cases above were found precisely *because* a
  bound was tight enough to fail. The goal is bounds that fail for their own
  reason and say so, not bounds that never fail.

## Acceptance

- The four rows pass under full-suite contention on the platforms they run on,
  and each new bound's derivation is recorded.
- The survey is done and its result recorded, whether it finds four or forty.
- The tree either has a stated rule for sizing these bounds, or this card
  records the decision not to have one and the reason.
- An expiry names itself as an expiry in the line a reader acts on, or the card
  says why that could not be generalised beyond [[BPR-0901]]'s two drivers.
- [[BPR-0901]] and [[BPR-0893]] are reachable from wherever the rule lands, so
  whoever meets the fifth instance finds the first four.
