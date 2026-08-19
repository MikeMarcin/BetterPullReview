---
id: BPR-0345
title: Resolve the nondeterministic epoch maintenance verify-deleted failure
status: Done
rfc: RFC-BPR-0003
completed: 2026-08-18
depends_on: []
---

# Resolve the nondeterministic epoch maintenance verify-deleted failure

`application_epoch_maintenance_public_path` fails nondeterministically. Run
it repeatedly with nothing changed between runs:

```sh
for i in $(seq 10); do
  ctest --test-dir build/dev -R '^application_epoch_maintenance_public_path$'
done
# Passed, Failed, Passed, Failed, then Failed, Passed, Failed, Failed, Failed, Failed
```

The failing run reports:

```
EPOCH_FIXTURE_REUSE_FAILED
EPOCH_MAINTENANCE_DELETED_RESTART_FAILED
PUBLIC_PATH_GROUPS:core=1 fixture=1 maintenance=0 render=1
PUBLIC_PATH_BUDGET:expired=0 budget_ms=60000 elapsed_ms=245
```

The budget is not expired and the elapsed time is a quarter of a second, so
this is not the slow-host timeout that BPR-0156 recorded against
`application_review_ledger_composition_contract`. It is a real assertion
failure.

## Corrected — it does not alternate, and the root is cleared

Two claims above were wrong and are corrected here rather than edited away.

The contract *does* clear its root on entry. Line 14 of
`epoch_maintenance_public_path_contract.cmake` is
`file(REMOVE_RECURSE "${BPR_ROOT}")`, so the leftover-state explanation is
not available.

It also does not alternate. Four runs happened to, which is what the card was
written from. Six more gave `Failed, Passed, Failed, Failed, Failed, Failed`.
It is nondeterministic, not state-toggled, and the earlier four were a
coincidence read as a pattern.

`TMPDIR` is genuinely not involved, and BPR-0341's repair did not change this,
so the two remain separate.

## What the failing run actually reports

The failing phase is `--epoch-fixture-phase reuse --epoch-maintenance-probe
verify-deleted`. It fails in about 250ms against a 60000ms budget that is not
expired, so it is not the slow-host timeout recorded against
`application_review_ledger_composition_contract`.

```
PUBLIC_PATH_GROUPS:core=1 fixture=1 maintenance=0
PUBLIC_PATH_MAINTENANCE_EVIDENCE:0:0:0:0:0:0:0:0:1:0:0:0:1:-1:-1
```

`core` and `fixture` pass; only `maintenance` fails. Decoding the vector
against the emitter, exactly two required fields are false:

| Field | Required | Observed on failure |
|---|---|---|
| `PruneIntentDispatchCount` | 0 | 0 |
| `DeleteIntentDispatchCount` | 0 | 0 |
| `TargetBaseCacheDeleted` | true | **false** |
| `TargetHeadCacheDeleted` | true | **false** |
| `RetiredRepositoryPresent` | false | false |
| `RetiredCacheDeletionMarkerPresent` | true | true |

The retired-cache marker is present and the repository is gone, so the
deletion did happen. What is not observed is the per-target deleted flags.
That shape — the effect present, the observation of it absent,
nondeterministically, a quarter of a second in — points at the probe sampling
before the state it reads has settled, rather than at a deletion that failed.

## A discrepancy to resolve while here

The assertion and the printout do not ask the same question. The
`verify_deleted` assertion calls
`maintenance_evidence(aApplication, { })` with a default-constructed
`review_epoch_id`, while the diagnostic line a few hundred lines later calls
`maintenance_evidence(aApplication, aRun.MaintenanceTarget)`.

Both report false here, so this is not the cause of the failure. It should
still be reconciled: an assertion about a target that passes no target is at
best relying on the field being target-independent, and nothing says it is.

## Required final state

- Establish whether the two flags are being read before the maintenance
  observation settles, and if so make the probe wait for the state rather than
  sample it.
- The assertion and the diagnostic query the same target.
- Twenty consecutive runs give twenty identical results on a stock Ubuntu
  26.04 host. Four was not enough to characterise this and ten would not have
  been either.
- Check macOS. Nothing here is Linux-specific on its face, and a race would
  simply be likelier or rarer there rather than absent.

## Superseded — the pre-correction requirements

These were written before the correction above and are kept only so the record
shows what changed. They ask for a clean-root/reuse decision that the
correction already settled (line 14 of the contract cmake clears the root), and
for ten runs, which the correction replaces with twenty. Do not work from them.

- The contract either starts from a clean root or genuinely proves reuse
  against the state it left. Whichever is intended, running it twice in a row
  produces the same verdict twice.
- The deleted-restart phase's expectation is stated against a defined starting
  state rather than an inherited one.
- Ten consecutive runs give ten identical results on a stock Ubuntu 26.04 host.
- Whether macOS alternates the same way is checked. Nothing here is
  Linux-specific on its face, so this may have been latent on every platform
  and only noticed because the Linux matrix ran the contract repeatedly.

## Result — 2026-08-18

The card's reading of the shape was right — "the effect present, the
observation of it absent" — and it pointed at the probe sampling before the
state settled. That was one of two defects, and repairing it exposed the other.

### The gate polled less than the assertion checked

`begin_epoch_maintenance_probe`'s `verify_deleted` branch required the
retired-cache marker, the retired repository's absence, the two dispatch
counts, and four provider counts. It did not require
`TargetBaseCacheDeleted` or `TargetHeadCacheDeleted` — the exact two fields the
closing assertion needs.

Returning true from that gate is what ends the polling and calls
`transition_to_shutdown`. So every condition the gate omitted was one the
assertion read from whatever state happened to exist at the moment the run
stopped waiting. The two flags settle later than the marker beside them: the
marker is written by the deletion itself, while the flags are read back from a
catalog that this process — a fresh one, started after the delete — must first
rediscover and rehydrate.

### The assertion was querying a torn-down catalog

Adding the flags to the gate did not fix it, and how it failed named the second
defect. The run still failed about half the time, now with `presentation=1` —
meaning the gate *had* been satisfied, both flags true, at 246ms — while the
assertion moments later still read them false.

`maintenance_evidence` is a live observation, not a stored fact. For a retired
target it reopens the catalog and reads the epoch's anchors back out of it. The
assertion runs after shutdown, once that catalog is gone, so it was not
re-checking the claim; it was asking a question of an object that no longer
exists, and the honest answer to that is the struct's `false` default.

The probe now records the two flags at the moment it is satisfied, and the
assertion reads that record. `MaintenanceObserved` keeps this from being a
tautology: it is set only where the probe actually observed both flags true, so
a run whose probe never settled arrives at the assertion with an empty record
and fails.

### The target discrepancy

Reconciled, and it was less benign than "both report false here" suggested. The
gate and the assertion passed a default-constructed `review_epoch_id` while the
diagnostic passed `aRun.MaintenanceTarget`. Those agreed only because
`verify_deleted` never assigns that field — a coincidence, not a contract, and
the other two probes do assign it. All three now name the same field, and the
field carries a comment saying why `verify_deleted` deliberately leaves it
default-constructed.

## Evidence

- Twenty consecutive runs, twenty identical results: `P` ×20. The card asked for
  twenty because four had been read as a pattern and ten would not have settled
  it either. Before the repair the same command over twenty runs gave
  `F P P F P P P P P P P F F F F P F F F F` — 10 of 20 — on the same host with
  nothing else changed.
- The gate is load-bearing, not decorative. Mutated so it can never be
  satisfied, the run does not pass and does not fail quickly: it ends on its
  budget, `expired=1 budget_ms=60000 elapsed_ms=60020`, `presentation=0`. That
  is the outcome a genuinely unsettling state now produces, and it is
  distinguishable at a glance from the 246ms failure this card started from.
- The record is load-bearing too. Mutated so the probe is satisfied but drops
  its record, the run reaches `presentation=1` and still fails with
  `maintenance=0`. So the assertion is not passing merely because the gate did.
- The wider epoch matrix passes 14/14, including both persistence courses and
  the catalog contracts.
- `src/app/main.cpp` is format-clean under clang-format 22.1.2, the version
  [[BPR-0367]] establishes the tree is formatted for.
- `python3 scripts/verify_documentation.py --root .` reports `DOCUMENTATION_OK`;
  `git diff --check` passes.

### Platform

Nothing in the repaired path is Linux-specific: the settling is core state plus
a portable SQLite read, and the polling is a Qt stage-machine retick with no
platform timer or filesystem watch. This was a plain ordering race that would
be latent on macOS and Windows and differ only in how often it lost. Only Linux
was executed here, as this session has no other host.

The card also asked to check whether macOS alternates the same way. That is not
answered here and should not be recorded as though it were: what is established
is that the defect is not platform-specific by construction, which is a weaker
claim than a macOS run.

## Correction — this slice missed the same gate BPR-0380 had just documented

The repair edits `src/app/main.cpp`, which is one of the eight sources the
production application manifest digest covers
(`application_review_round_trip_composition_contract.cmake:135`). The slice did
not re-pin it, so `application_composition_contract` began failing on the
digest immediately after this card closed.

That is precisely the failure mode [[BPR-0380]] had recorded one slice earlier,
against `27e4bed`, and it recurred anyway. The reason is the same one BPR-0380
named: this card's verification ran the selectors the card named, and the
composition contract is not among them, so nothing here was going to notice.

The pin is corrected to `b6d2a2c6…`. The other two pins are untouched: the
file digest at line 42 still matches, because `github_application.cpp` did not
change, and the narrower mutation pin did not drift.

The recurrence is evidence for BPR-0380's closing observation rather than
against it. A per-slice rule that depends on remembering which shared gate a
file participates in will be missed again; `src/app/main.cpp` carries nothing
that says a digest covers it.
