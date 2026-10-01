---
schema: "implement-rfc.card.v1"
id: "BPR-0905"
title: "Four more bounds are sized for a quiet host, one has already expired, and nothing says what margin a bound needs"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-007, from BPR-0901's fix and bpr-linux's BPR-0880 full-suite runs","found_at":"22023667","run":"run-20260916-stations","dispatch":"dsp-007","milestone_plan":{"id":"bpr0905-run-bounds","path":"docs/rfcs/plans/bpr0905-run-bounds/milestone-plan.json"}}
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

## A margin cannot be read from a green run, and that is half the reason these persist

`src/app/main.cpp:4379` prints `PUBLIC_PATH_BUDGET:… elapsed_ms=…` **only
inside the failing-verdict block**. A run that stays inside its budget emits no
elapsed figure at all.

So there is no way to ask a healthy host how close it came. This dispatch asked
all three platforms for an exact `elapsed_ms` on the two [[BPR-0901]] rows and
the request was unanswerable everywhere they passed; row wall time is the only
proxy, and it covers all four runs rather than the worst one. **A bound whose
margin is invisible until it is breached is a bound nobody can maintain** —
which is exactly how four of these accumulated. Printing the elapsed figure
unconditionally is a small product change and it belongs with this card's third
option, because the two together make a thin bound both visible and
self-naming.

## The exposure is not uniform, and nobody should raise a bound without that

Linux carries **sixteen** rows on the 40000 ms fixture-run bound and **all
sixteen passed** under `-j10` sweep load. The extremes are
`uiforetw_selected_diff` at 18.36 s and
`anonymous_public_preview_ghes_refusal` at 0.43 s. Critically,
`application_saved_server_contract` — the row named above as already recorded
expiring at a 1.7x margin — **passed here at 11.21 s**.

The two [[BPR-0901]] rows took 10.53 s and 11.59 s under that same load, whole
row, against a 40 s *per-run* bound: roughly fourfold clear on Linux, while
macOS crossed the line by 35 and 52 ms and Windows passed at 44 s a row.

That is the shape of the problem. These bounds are not uniformly thin — they
are thin **on one host**, and the host that crosses first is not the same for
every row. A survey that measures one platform will conclude there is nothing
here.

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

## 2026-09-25 Mac implementation and pending platform verification

The unattended owner mandate permits independent implementation on the Mac;
new Windows/Linux verification is explicitly deferred. This partial result
does not satisfy the full-suite/all-platform acceptance criterion. The plan is
`bpr0905-run-bounds`, milestone `honest-run-bounds`, on baseline `97a7564b`.

### Changes and derivation

- Saved-server, defect and ledger seed/reuse phases now use the existing
  120,000 ms epoch class: respectively 5.1x, 6.7x and 6.5x the original
  recorded quiet phase measurements. The ledger's unavailable/single phase
  keeps 40,000 ms. Their CTest allowances are 660, 660 and 500 seconds,
  derived from the existing 75% harness rule with four/four/three maximum
  run allowances and the unchanged 4-second reserve. The actual current
  drivers have four defect runs and three ledger runs, including the latter's
  single-phase refusal course.
- SQLite's row timeout is 420 seconds: three times the recorded Linux
  isolated 122.65 seconds, rounded upward to the next minute. Recorded
  180.03-second expiries remain lower bounds on completion, not an invented
  contended measurement. This larger bound still needs native contention proof.
- All application automated terminals expose budget, elapsed time and ticks.
  One elapsed sample serves the expiry and budget lines; elapsed includes
  shutdown, while the expiry flag records expiry during the course.
  `RUN_BUDGET_EXPIRED` precedes the generic failed marker. The three corrected
  drivers use a shared result reporter that forwards successful timing and
  names expiry or CMake's child-process timeout in its failure headline.
- [The authoring rule](../../development/build-and-test.md#run-budgets-and-row-timeouts)
  records sizing, host overload and result reporting. The
  [survey](../../development/run-budget-survey-2026-09-25.md) joins the current
  449-row Mac graph to 1,029 historical logs: 299 explicitly bounded rows,
  244 with historical passing times, 55 unknown margins, and 21 additional
  rows below 3x the largest recorded pass. Those 21 bounds remain unchanged.
  The native Windows/Linux-only inventories and missing success phase times
  remain explicit coverage limits.

### Self-review and source-pin reseal

Reviewed `97a7564b` through this implementation's diff. The only production
application source change is `main.cpp`'s automated terminal report: a single
monotonic elapsed sample, a budget line also emitted on success, and expiry
before the failed-run marker. No course scheduling, readiness, verdict,
shutdown, credentials, provider selection, authority or mutation logic changed.
Existing contiguous success-marker expectations retain their ordering after
the new preceding budget line.

The production application manifest pin is resealed in this same commit to
`394acf6599072cf46bd80e45a56d0ce1557bf0d15b56ab41892d5695c535fb47`.
The full `github_application.cpp` and production mutation branch pins remain
unchanged. The script's own print-only digest computation agrees. No other
pinned-scope source changed since the baseline. The CMake helper accepts the
actual numeric/non-numeric `execute_process` result vocabulary; expiry remains
fatal even if a child incorrectly returns zero.

Semantic documentation disposition: this affects automated course budgets and
diagnostics only. The build/test guide and survey own that contract; runtime
review behavior, public APIs, identity, persistence, provider and package
contracts have no semantic change.

### Candidate verification and acceptance boundary

Product candidate `eb55ff7f` (documentation-only continuation `50e33ccb`),
macOS 26.6.2 arm64, Qt 6.11.2, Debug:

- The four corrected rows passed 4/4, 218.81 seconds total: SQLite 40.84 s,
  ledger 36.34 s, defect 51.22 s, saved-server 90.38 s. The initial ledger
  single phase had the temporary maximum allowance in that pass; its final
  split was then verified independently as below.
- Final integration passed 5/5: ordinary public path, production composition
  including its tests-disabled Release application, final ledger, source-pinned
  review round-trip, and positive/stalled automated-budget contract. Final
  ledger phases recorded 10,846/12,476 ms at 120,000 ms and its unavailable
  phase 10,645 ms at the unchanged 40,000 ms.
- Forced 1 ms runs of each affected driver failed with `RUN_BUDGET_EXPIRED`
  in the driver's headline. The generic result helper's five controls passed
  (success, expiry, expiry with a false zero exit, process timeout, ordinary
  failure). No live provider was contacted.
- Source formatting and scoped main.cpp tidy pass. Documentation gate, board
  validation, 64 documentation verifier tests, and network manifest pass.
  The manifest's first invocation failed because a required audit executable
  was not in this partial build; building that exact target resolved it.

Runtime logs and machine-readable control results are retained under
`/Users/jaeger/.codex/tmp/bpr0905-3mteyp25`. The
[durable handoff](../../rfcs/plans/bpr0905-run-bounds/HANDOFF-2026-09-25.md)
records commands, candidate scope, source review and continuation.

**Remaining:** AC1's full-suite contention runs on Windows and Linux
are unperformed. The Mac run at integrated candidate `f6de40b6` passed all
four target rows under `ctest --preset dev -j10`; its other failures and exact
limits are recorded in the [Mac sweep](../../verification/BPR-0905-mac-full-suite-2026-09-25.md).
The survey records the available Mac
graph and historical station logs; native-only inventory and passing-run
phase margins without observations remain unknown. Windows/Linux runs were
explicitly deferred by the owner. The card returns to Backlog, with the
implementation and Mac contention evidence complete. Cross-platform
acceptance is not claimed.

The Mac sweep also exposed a separate release HTTPS fixture deadline: its
four-hop redirect-cap course shared the 500 ms allowance used by single-request
courses. The [sweep follow-up](../../verification/BPR-0905-mac-full-suite-2026-09-25.md#release-https-fixture-deadline-follow-up)
records the fourfold course-specific allowance and focused Mac pass. It does
not change the four BPR-0905 target rows or satisfy their pending Windows/Linux
contention evidence.

## 2026-10-01 takeover and cross-platform checkpoint

Claude rfc-implementer took over the stale Codex worktree
`codex/bpr0905-budgets` (`ed884bd6f`). Nothing needed to be carried: every
commit on that branch is patch-equivalent to one already on main
(`07befacef`..`f6de40b63`, followed there by `0f0c461e0` and `49f4c1ec4`).
The worktree was read, not edited. The owner's 2026-10-01 amendment allows the
Windows and Linux stations to be used, so the earlier deferral of those runs
no longer applies. The card was promoted to Ready (`723fe3a61`) and claimed
(`4c3205c09`).

**Candidate** `4c3205c09`. Its product tree is identical to `a3c2aa88c`.

**Mac: done.** `ctest --preset dev -j10` ran all 577 rows in 717.73 s: 575
passed. All four rows passed (saved-server 19.41 s, defect 10.95 s, ledger
7.35 s, SQLite 44.96 s). Every per-phase `PUBLIC_PATH_BUDGET` line shows
`expired=0` and was visible in the successful log, which also confirms the
success-timing criterion on the full suite. The two failures were not clock
failures and are already owned by BPR-1245 and BPR-1236. The
[full-suite record](../../verification/BPR-0905-full-suite-2026-10-01.md)
holds the per-phase figures and a fresh Mac margin table. In that table,
`live_mutation_isolation` (1.15x) and `diagnostic_context_age` (1.16x) are
newly thin. They are recorded as follow-up candidates, and neither bound was
changed.

**Remaining: Windows and Linux.** AC1 still needs the four rows under
full-suite contention on `bpr-main-win` and `bpr-main-linux`. The survey also
still lacks the native inventory margin table from each of those platforms.
The exact commands, and what each station must return, are in that record's
Windows and Linux sections. AC2 through AC5 are satisfied on main: the survey,
the sizing rule in `build-and-test.md` with its BPR-0901 and BPR-0893 links,
and self-naming expiry with forced-expiry controls. If a station returns a
clock failure on one of the four rows, re-derive that bound from the returned
measurement using the rule; do not retry it until it passes. Once both
stations' results are recorded in the full-suite record, finish the card.
