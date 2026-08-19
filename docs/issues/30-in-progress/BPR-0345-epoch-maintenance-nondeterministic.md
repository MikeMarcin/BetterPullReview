---
id: BPR-0345
title: Resolve the nondeterministic epoch maintenance verify-deleted failure
status: In Progress
rfc: RFC-BPR-0003
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

## Required final state

- The contract either starts from a clean root or genuinely proves reuse
  against the state it left. Whichever is intended, running it twice in a row
  produces the same verdict twice.
- The deleted-restart phase's expectation is stated against a defined starting
  state rather than an inherited one.
- Ten consecutive runs give ten identical results on a stock Ubuntu 26.04 host.
- Whether macOS alternates the same way is checked. Nothing here is
  Linux-specific on its face, so this may have been latent on every platform
  and only noticed because the Linux matrix ran the contract repeatedly.
