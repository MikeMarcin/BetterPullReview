---
schema: "implement-rfc.card.v1"
id: "BPR-0901"
title: "macOS: two epoch fixture courses fail intermittently, and they alternate"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-006 integrator, across four separate macOS runs at the merged head","found_at":"81844fef","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"The card names which of the three hypotheses is true, with the runs that showed it","outcome":"pass","revision":"22023667b430c78119d0299703785dfb76fa4fdf","evidence":{"kind":"observation","detail":"None of the three. Both rows drive --epoch-fixture-phase four times but carried bpr_fixture_run_budget_ms (40000 ms) instead of bpr_epoch_phase_budget_ms (120000 ms), against a phase measured at 28 s on a quiet host. Eleven recorded expiries across four dispatches in build/r2-logs, every one within 53 ms of the bound, including RUN_BUDGET_EXPIRED:elapsed_ms=40035 and 40052 quoted on the card and confirmed independently by the integrator. Hypothesis 1 is disproved by a log dated 2026-09-13 at 4a2be1b1, four days before 81844fef, showing the same expiry before the data root moved."}},{"criterion":"Both rows pass repeatedly, with the count and the reason it is enough","outcome":"pass","revision":"22023667b430c78119d0299703785dfb76fa4fdf","evidence":{"kind":"command","detail":"After the change: 3 consecutive ctest -j2 iterations of both rows, 6/6 pass at 92-94 s; and the full 383-row sweep, 380/383 in 1397 s, with #343 at 101.16 s and #344 at 99.17 s. Log build/r2-logs/dsp7-0901-full-sweep.log. The count is not the argument: the rows were green 22 times before the change too. The argument is that the failure is a measured quantity that moved from 28 s against 40 s to 28 s against 120 s, and that the sweep contains no budget_ms=40000 expiry in 383 rows."}},{"criterion":"If the cause is shared with the other ten changed courses, they are checked too","outcome":"pass","revision":"22023667b430c78119d0299703785dfb76fa4fdf","evidence":{"kind":"observation","detail":"The data root is not the cause, so the ten BPR-0875 courses are the wrong family. The right family is rows driving --epoch-fixture-phase on a budget that is not bpr_epoch_phase_budget_ms. Three remain: application_saved_server_contract (1.7x margin, already recorded expiring), application_defect_composition_contract and application_review_ledger_composition_contract (2.2x). Filed as BPR-0905 with a fourth instance from bpr-linux. Rows using --epoch-fixture-phase single are a 12-13 s render and are correctly budgeted."}},{"criterion":"Linux and Windows are checked for the same fault, or the card says why they cannot be affected","outcome":"pass","revision":"22023667b430c78119d0299703785dfb76fa4fdf","evidence":{"kind":"observation","detail":"Both registrations in tests/end_to_end/CMakeLists.txt are not platform-conditional, so all three trees carried the same 40 s budget. bpr-linux quoted both registrations verbatim from its own HEAD, confirming it structurally without running anything. The expiry is wall-clock against Git subprocess and SQLite work, so a slower or busier host reaches it sooner. Both stations are told to grep their sweep log for RUN_BUDGET_EXPIRED with budget_ms=40000 rather than diagnose it, and to reconfigure after this lands because the budget is a configure-time argument."}}]
---

# BPR-0901: macOS: two epoch fixture courses fail intermittently, and they alternate

## Defect

`application_overview_since_review_contract` and
`application_review_behaviour_contract` each fail **some** of the time on
macOS, at the epoch fixture's seed or reuse phase. Neither fails reliably, and
in four runs at the same commit they never failed together — when one failed,
the other passed.

| Run | #342 `overview_since_review` | #343 `review_behaviour` |
|---|---|---|
| 13 affected rows, merge verification | **pass**, 95.41 s | **FAIL**, 40.50 s |
| #343 alone, isolated | — | **pass**, 93.86 s |
| full 382-row sweep | **FAIL**, 117.82 s | **pass** |

The two failures are the same shape:

- **#342**: `EPOCH_FIXTURE_REUSE_FAILED`,
  `PUBLIC_PATH_EPOCH_STATUS:Review history is unavailable.`,
  `PUBLIC_PATH_EPOCH_CORE_STATUS:7`, `RENDER_FAILED`, child exit 1.
- **#343**: `EPOCH_FIXTURE_SEED_FAILED`,
  `EPOCH_EFFECT_FAILED:revision-prepare:5`,
  `GITHUB_FIXTURE_FAILED:github.com`, `REVIEW_LEDGER_FAILED`, child exit 1.

Both print `PUBLIC_PATH_FAILED`, which is a `FAIL_REGULAR_EXPRESSION`, so these
rows would have failed the same way before [[BPR-0890]]'s exit-code work — the
new instrumentation is **not** what fails them. It is worth saying that plainly,
because both failures were first seen in a tree that also carried BPR-0890's
work in progress, and the obvious reading was wrong.

## What is known, and what the discriminating experiment is

Both rows are among the **twelve courses [[BPR-0875]]'s fix changed** at
`81844fef`. Each gained `bpr_private_data_root(BPR_ROOT)`, which moves the data
root down one level (`<binary>/<row>` becomes `<binary>/<row>/data`) and makes
the new parent mode 0700 on POSIX. Their fixture roots are derived from
`BPR_ROOT` *after* the rewrite, so no stale absolute path survives — but the
paths are longer and the parent's mode changed, and this dispatch has already
been bitten once by a path-length limit ([[BPR-0881]]).

Against that: dsp-005's macOS sweep at `2b2e5549` was 378 of 381 with **only**
the three [[BPR-0893]] footprint rows failing, so neither of these failed there.
That is one data point, not a proof of stability — this dispatch ran these two
rows more times in one afternoon than the run had before, which is its own
explanation for why an old flake would surface now.

The experiment that separates the three hypotheses in one pass:

1. run both rows several times at `HEAD`;
2. run both rows several times with **only** those two
   `tests/end_to_end/*.cmake` course scripts reverted to their pre-`81844fef`
   content.

That distinguishes "the twelve-course change made them flaky" from "they were
always flaky" from "something else in this dispatch is involved". Do that
before changing anything.

## Scope

- Run the experiment above first. Do not fix on a hypothesis.
- If the extra level or the 0700 parent is implicated, check the other ten
  changed courses too rather than only these two — they took the same change.
- If it is a pre-existing flake, find what is actually racing.
  `EPOCH_EFFECT_FAILED:revision-prepare:5` and
  `PUBLIC_PATH_EPOCH_CORE_STATUS:7` both name specific states and are the
  thread to pull.
- Whatever the cause, these two rows are the suite's own evidence about epoch
  fixtures. An intermittent row is worse than a red one, because it teaches
  people to rerun.

## Acceptance

- The card names which of the three hypotheses is true, with the runs that
  showed it.
- Both rows pass repeatedly — say how many consecutive runs were taken as
  enough, and why.
- If the cause is shared with the other ten changed courses, they are checked
  too.
- Linux and Windows are checked for the same fault, or the card says why they
  cannot be affected.

## What was measured — none of the three

The answer is a fourth hypothesis, and it is not a race at all. **Both rows are
epoch fixture rows carrying the fixture *render* budget.** Each drives
`--epoch-fixture-phase` four times — seed and reuse, for GitHub.com and for
GHES — and each phase seeds a Git object cache and a SQLite catalog for real.
That phase costs **28 s** driven directly on a quiet macOS host. Its budget was
`bpr_fixture_run_budget_ms`, **40 s**. A 1.4x margin, against work [[BPR-0555]]
already measured at 33-35 s on a loaded host and 60-68.6 s on a busy one.

So on a host doing anything else the phase runs past 40 s, the application
stops *itself* — that is what the budget is for — and reports the stage it was
in. The stage is the epoch fixture, so the row prints `EPOCH_FIXTURE_SEED_FAILED`
or `EPOCH_FIXTURE_REUSE_FAILED` and reads as an epoch defect. It is a clock.

This is [[BPR-0555]] exactly, in rows [[BPR-0555]] did not sweep. That card
moved four rows to `bpr_epoch_phase_budget_ms` (120 s) and caught a fifth by
hand; seventeen sites use that budget today. These two were missed.

### The evidence was already on disk

Every recorded failure of either row carries its own verdict. The driver dumps
the child's whole marker block, and `RUN_BUDGET_EXPIRED` is in every one of
them — under a headline that says "process failed", which is the line a reader
acts on. That is why four dispatches read these as epoch defects.

| Log, line | Row | Phase | Recorded line |
|---|---|---|---|
| `dsp6-merge2-ctest.log:27` | `review_behaviour` | github.com seed | `RUN_BUDGET_EXPIRED:elapsed_ms=40052 budget_ms=40000 over_ms=52 stage=2` |
| `dsp6-0890-sweep.log:340` | `overview_since_review` | ghes-3.18 reuse | `RUN_BUDGET_EXPIRED:elapsed_ms=40035 budget_ms=40000 over_ms=35 stage=12` |
| `int3-ctest.log:48` | `review_behaviour` | github.com seed | `RUN_BUDGET_EXPIRED:elapsed_ms=40049 budget_ms=40000 over_ms=49 stage=4` |
| `int3-rerun-application_review_behaviour_contract.log:17` | `review_behaviour` | github.com seed | `RUN_BUDGET_EXPIRED:elapsed_ms=40046 budget_ms=40000 over_ms=46 stage=4` |
| `dsp2-0818-contract-4a2be1b1.log:17` | `review_behaviour` | github.com seed | `RUN_BUDGET_EXPIRED:elapsed_ms=40048 budget_ms=40000 over_ms=48 stage=4` |
| `dsp2-0818-siblings-a445866d.log:17` | `overview_since_review` | github.com seed | `RUN_BUDGET_EXPIRED:elapsed_ms=40049 budget_ms=40000 over_ms=49 stage=4` |

All under `build/r2-logs/`. The first two rows of that table are this card's
own two failures. Nine expiries of `review_behaviour` are on disk across four
dispatches, plus two of `overview_since_review`; every one lands within 53 ms
of the bound, which is what a clock looks like and what a race does not.

**`dsp2-0818-contract-4a2be1b1.log` is at `4a2be1b1`, dated 2026-09-13 — four
days before `81844fef`.** The row expired the same way before the data root
moved. That disposes of hypothesis 1 from a log rather than from an absence of
failures, which matters given [[BPR-0893]]'s finding that this build directory
can produce a pass-then-fail at one fixed revision.

### The runs, with their load condition

Machine load is part of each result, because it is the variable that decides
these rows. All at `22023667` in `build/r2-dev`.

| Phase | Context | Load | Result |
|---|---|---|---|
| 1 | both rows, `ctest -j2`, 4 iterations | integrator also testing part of the time | 8/8 pass, 92-94 s |
| 1 | both rows, `ctest -j2`, 3 iterations | 14 CPU spinners | 6/6 pass, 107-109 s |
| 1 | the 13 affected rows, `ctest -j10`, 4 iterations | idle | 8/8 pass, 97-100 s |

Phase 2 — reverting the two course scripts to their pre-`81844fef` content —
**was not run, deliberately.** A revert is only worth measuring against a
reproduction, and at HEAD on this machine the rows do not fail: 22 green row
results across three contexts before the fix, in both of the contexts they
originally failed in. Reverting to chase a failure that is not currently
producible would have measured nothing, and it would have left twelve shared
courses modified in a tree the Linux station is being compared against. The
dated log above answers hypothesis 1 outright, and it is better evidence than
a revert would have been.

What the rows do reproduce on demand is the mechanism. Driven directly with the
budget forced below the phase cost, the marker chain the failures show is
produced exactly, including which markers appear: an early expiry gives
`GITHUB_FIXTURE_FAILED` and `REVIEW_LEDGER_FAILED`, a late one gives them `OK`.

| Forced budget | Recorded | Child exit |
|---|---|---|
| 10000 ms, seed | `elapsed_ms=10046 … stage=2`, `EPOCH_FIXTURE_SEED_FAILED`, `GITHUB_FIXTURE_FAILED`, `REVIEW_LEDGER_FAILED` | 1 |
| 20000 ms, seed | `elapsed_ms=20048 … stage=7`, `EPOCH_FIXTURE_SEED_FAILED`, `GITHUB_FIXTURE_OK` | 1 |
| 12000 ms, reuse | `elapsed_ms=12051 … stage=10`, `EPOCH_FIXTURE_REUSE_FAILED` | 1 |
| 18000, 22000 ms, reuse | `EPOCH_FIXTURE_REUSE_OK` | 0 |
| 40000 ms (as configured), seed and reuse, quiet host | no expiry; the phase takes 28 s | 0 |

### One correction to this card's own Defect section

The `#343` marker chain above — `EPOCH_EFFECT_FAILED:revision-prepare:5`,
`GITHUB_FIXTURE_FAILED:github.com`, `REVIEW_LEDGER_FAILED` — **is not what the
log recorded.** `dsp6-merge2-ctest.log` shows that failure with
`PUBLIC_PATH_EPOCH_CORE_STATUS:6`, `GITHUB_FIXTURE_OK:github.com`,
`REVIEW_LEDGER_OK` and `RENDER_OK`, and no `EPOCH_EFFECT_FAILED` at all;
`revision-prepare:5` appears only in dsp-004's negative-control logs for
another card. Those three markers sent the reading toward the epoch path. The
marker that was present in both failures, and omitted from the card, is
`RUN_BUDGET_EXPIRED`.

## Landed

**The budget.** Both rows move from `bpr_fixture_run_budget_ms` to
`bpr_epoch_phase_budget_ms`: 40 s → 120 s, 4.3x the 28 s quiet measurement.
The argument for that multiple is not that it is safer; it is that it makes
these two ordinary among the seventeen epoch-phase sites instead of a pair with
a bound of their own.

**The timeout, and only because it must move.** `bpr_bound_automated_run`
refuses 4 × 120000 + 4000 ms inside a 240 s timeout — the rule is that the
worst case fits in three quarters of the harness allowance — so the timeout
rises to 660 s, whose three-quarter share (495000 ms) covers the 484000 ms
worst case. `RESERVE_MS` stays at 4000: nothing outside the budget changed.
The rows' actual wall time is 93-94 s.

**The headline.** Both drivers now lift `RUN_BUDGET_EXPIRED` into the line
under the failure, which is the half of [[BPR-0555]] these two never got.
Proven by driving the course with the budget forced to 10 s: the message reads
`review behaviour github-com seed process failed (1)` and then
`RUN_BUDGET_EXPIRED:elapsed_ms=10054 budget_ms=10000 over_ms=54 stage=2`.

### Verified

At `22023667` in `build/r2-dev`, on an idle machine, after the change:

| Run | Result |
|---|---|
| both rows, `ctest -j2`, 3 consecutive iterations | 6/6 pass, 92-94 s |
| the full 383-row sweep, `ctest -V -j10`, 1397 s | **both rows pass** — #343 101.16 s, #344 99.17 s |

Three consecutive iterations plus the sweep is the count taken as enough, and
the reason is not the count. Repeated green proves little here on its own:
[[BPR-0893]] showed this build directory producing pass-then-fail at one fixed
revision, and these rows were green 22 times *before* the change too. What
makes the fix answerable is that the failure is a measured quantity rather
than an outcome — the elapsed against the bound, recorded in every failure —
and that quantity moved from 28 s against 40 s to 28 s against 120 s. The runs
confirm the rows still pass at the new bound; the arithmetic is what says they
will keep passing on a host that is merely busy. The sweep also carries no
`budget_ms=40000` expiry anywhere in 383 rows: the one `RUN_BUDGET_EXPIRED` it
contains is the deliberate 5000 ms stall row.

### This dispatch's macOS sweep

Reported here because the sweep is the context one of these rows failed in, so
the verification run and the dispatch sweep are the same run. `380/383`, 1397 s
real, `CTEST_RC=8`, log at `build/r2-logs/dsp7-0901-full-sweep.log`:

| Row | Child exit | Owner |
|---|---|---|
| `test_network_isolation_structure_contract` | 1 | not this card — `verify_test_network_isolation_tests.py` pins exemption causes and two are unaccounted: `repository-content-outside-checkout`, `child-process-denied` |
| `application_large_review_cache_bytes` | 1 | [[BPR-0893]] |
| `application_large_review_resources` | 1 | [[BPR-0893]] |

`application_large_review_loaded_close` **passed** here at 13.55 s having
failed in dsp-006's sweep, which is [[BPR-0893]]'s own instability showing
again and is that card's to read, not this one's.

### The other rows

Not the other ten changed courses — the data root is not the cause, so that is
the wrong family. The family is rows that drive `--epoch-fixture-phase` on a
budget that is not `bpr_epoch_phase_budget_ms`. There are three left, and one
of them has already been recorded expiring:

| Row | Phases | Measured | Margin at 40 s |
|---|---|---|---|
| `application_saved_server_contract` | 4 (seed+reuse × 2 fixtures) | 94.88 s total, ~23.7 s each | **1.7x** — expired at `dsp2-0818-siblings-a445866d.log:162` |
| `application_defect_composition_contract` | 3 (seed, seed, reuse) | 53.89 s total, ~18.0 s each | 2.2x |
| `application_review_ledger_composition_contract` | 2 (seed, reuse) | 36.77 s total, ~18.4 s each | 2.2x |

`application_saved_server_contract` is the same row shape as these two with the
same margin, and it has already failed this way once. It is left alone here
because it is outside this card, not because it is well.

Three further rows pass `--epoch-fixture-phase single`, which is the 12-13 s
render the 40 s budget was measured for, so they are correctly budgeted —
though `application_workbench_composition_contract` is recorded expiring at
`dsp4-linux-sweep-mac-classify.log:262` with `elapsed_ms=40020`, on a host
where an epoch-budget row expired at `budget_ms=120000` in the same run. That
host was in a state no budget in this file survives.

### Linux and Windows

`tests/end_to_end/CMakeLists.txt` is not platform-conditional here: before this
change, **both stations carried the same 40 s budget on the same two rows**,
and both should expect the same intermittent failure under load. Nothing about
the expiry is macOS-specific — it is wall-clock against work dominated by Git
subprocesses and SQLite, and a slower or busier host reaches it sooner.

Neither can be tested from here. What a station should run is a grep, not a
test: `grep -n "RUN_BUDGET_EXPIRED" <its sweep log>`. Any line reading
`budget_ms=40000` on either of these rows is this defect, and the reported
stage marker will name the epoch fixture rather than the clock. After this
change lands both stations must reconfigure, because the budget is a configure
-time argument to the course script.
