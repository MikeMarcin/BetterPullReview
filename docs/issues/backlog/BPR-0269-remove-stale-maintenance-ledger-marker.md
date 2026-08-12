---
id: BPR-0269
title: Remove the stale epoch-maintenance ledger marker
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0264]
---

# Remove the stale epoch-maintenance ledger marker

Once BPR-0266 restored the epoch-refresh presentation gate, the maintenance
course reached `PUBLIC_PATH_OK` and `EPOCH_MAINTENANCE_PRUNE_OK` but its wrapper
still rejected `REVIEW_LEDGER_PRESENT_ON_ENTRY:0`. The durable ledger database
and receipt are present. That marker counts receipt load hits through a selected
material observation, while the maintenance branch intentionally begins prune
before the selected-diff ledger probe. Its old expectation therefore describes
a path the current maintenance course does not execute.

## Required final state

- The epoch-maintenance wrapper asserts only evidence produced by the real
  maintenance path and no longer treats a receipt-load counter as database-
  existence proof.
- Seed and prune/reuse still use the same fixture root, persist the epoch and
  ledger stores, and prove the exact prune intent, retained comparison, cache
  effect, and terminal maintenance verdict.
- The dedicated review-ledger composition course remains the owner of exact
  seed/reopen receipt-load persistence and passes unchanged.
- Documentation distinguishes durable ledger persistence from a runtime
  selected-material load hit.

## Non-goals

- Do not change ledger storage, epoch maintenance policy, fixture-root cleanup,
  or application control flow merely to manufacture the stale marker.
- Do not weaken the dedicated ledger composition contract.
- Do not execute a credentialed/live target or claim Windows/Linux evidence.

## Verification

- Run `application_epoch_maintenance_public_path` and
  `application_review_ledger_composition_contract` together.
- Run the review-ledger host, epoch host, and epoch-maintenance integration
  component selectors plus architecture boundaries.
- Run repository format/tidy, documentation, and diff gates.
- Review the exact wrapper/ledger ownership change independently.
