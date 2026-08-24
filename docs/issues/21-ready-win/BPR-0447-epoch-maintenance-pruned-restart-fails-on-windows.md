---
id: BPR-0447
title: The epoch-maintenance pruned-restart run fails on Windows
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# The epoch-maintenance pruned-restart run fails on Windows

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
