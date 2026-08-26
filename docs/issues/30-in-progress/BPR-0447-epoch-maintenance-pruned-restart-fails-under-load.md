---
id: BPR-0447
title: The epoch-maintenance pruned-restart run fails under parallel load
status: In Progress
rfc: RFC-BPR-0003
depends_on: []
---

# The epoch-maintenance pruned-restart run fails under parallel load

`application_epoch_maintenance_public_path` fails identically in three
target-native Windows runs on 2026-08-23 (the settled full matrix and two
focused reruns, one with and one without Git's `usr/bin` on `PATH`). These
are the contract's first real executions on Windows: every earlier run was
refused by the host policy before capturing anything.

Measured, from the contract's own output:

- `EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK process failed (1)` — the
  restart-after-prune application run exits 1.
- `PUBLIC_PATH_STAGE:14`, `PUBLIC_PATH_VERDICT:presentation=0
  shutdown_complete=1`, `PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0
  render=1`.
- The restart run resolves no exact pair: `PUBLIC_PATH_EPOCH_PAIR:::-1:-1:2:0`,
  `PUBLIC_PATH_EPOCH_CORE_STATUS:-1`, epoch status stuck at "Resolving the
  exact comparison…" while `PUBLIC_PATH_STATUS` reads "Pull request ready."
- `EPOCH_FIXTURE_REUSE_FAILED`, `REVIEW_LEDGER_PRESENT_ON_ENTRY:0`,
  `REVIEW_LEDGER_FAILED`, and an all-zero
  `PUBLIC_PATH_MAINTENANCE_EVIDENCE` ending `:-1:-1`.
- The non-restart epoch flows pass on this host:
  `application_epoch_persistence_github-com`,
  `application_epoch_persistence_ghes-3_18`, and the composition contracts
  all passed in the same matrix.

## Required final state

- The failure is diagnosed to a cause on this host, not a symptom string.
- The cause is repaired on its own card if it is product code, or here if it
  is contract/fixture environment; the contract then passes target-native.
- No assertion is weakened and no Windows exemption is added to make it pass.

## Non-goals

- Not the Smart-App-Control refusal class (owner-deferred on [[BPR-0036]]).
- Not RFC-BPR-0002 closeout; none of its selectors depend on this contract.

## It is not a Windows failure — reproduced on macOS, 2026-08-26

Found by [[BPR-0454]]'s closing `dev` matrix on this macOS host, which returned
217 of 218 with this contract as the only failure. It then reproduced
deliberately: **two of three** runs failed under 40 spinning processes on a
14-core host, and it passes in isolation on the same commit (57.25 s, exit 0).
So the trigger is parallel load, not the operating system.

Every marker this card recorded from Windows appears identically here:

```
EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK process failed (1)
EPOCH_FIXTURE_REUSE_FAILED
EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_FAILED
PUBLIC_PATH_STAGE:14
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0
PUBLIC_PATH_GROUPS:core=0 fixture=0 maintenance=0 render=1
PUBLIC_PATH_STATUS:Pull request ready.
PUBLIC_PATH_EPOCH_PAIR:::-1:-1:2:0
PUBLIC_PATH_EPOCH_CORE_STATUS:-1
PUBLIC_PATH_MAINTENANCE_EVIDENCE:0:0:0:0:0:0:0:0:0:0:0:0:0:-1:-1
REVIEW_LEDGER_PRESENT_ON_ENTRY:0
REVIEW_LEDGER_FAILED
```

That is the whole list above, including the all-zero maintenance evidence
ending `:-1:-1` and the epoch status stuck at "Resolving the exact
comparison…" while the status line reads "Pull request ready." A Windows
session is therefore not required to diagnose this, and the card moves to
`20-ready/`.

The Windows observation stands as the first sighting; what it does not
support is the attribution. The three Windows runs were the contract's first
real executions on that host, so there was nothing to compare them against —
and this host had never run it under enough load to see it either.

### What this changes about the card

- The trigger to reproduce is parallel load, not a platform. Reproduce with
  the contract under a loaded matrix, not with a bare focused run.
- The required final state is unchanged, and the prohibition on a Windows
  exemption now reads more strongly: there is no platform to exempt.
- The "non-restart epoch flows pass on this host" observation held on Windows
  and holds here.
