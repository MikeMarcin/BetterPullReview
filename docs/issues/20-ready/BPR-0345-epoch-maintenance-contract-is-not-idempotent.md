---
id: BPR-0345
title: Make the epoch maintenance contract idempotent across consecutive runs
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# Make the epoch maintenance contract idempotent across consecutive runs

`application_epoch_maintenance_public_path` alternates. Run it repeatedly and
it passes, fails, passes, fails, with nothing changed between runs:

```sh
for i in 1 2 3 4; do
  ctest --test-dir build/dev -R '^application_epoch_maintenance_public_path$'
done
# Passed, Failed, Passed, Failed
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

Alternating rather than failing randomly is the useful part of the signal: the
contract leaves state behind that the next run trips over, and the failing run
then clears it, so the run after that succeeds again. The contract registers a
fixed root under the build tree and does not remove it on entry, unlike
`application_queue_composition_contract`, which begins with
`file(REMOVE_RECURSE "${BPR_ROOT}")`.

`TMPDIR` is not involved. The alternation is unchanged with `TMPDIR` on the
build filesystem, which is what separates this from BPR-0341. An earlier pass
under that variable was the alternation, not the fix.

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
