---
id: BPR-0128
title: Composition-root wiring and public-path evidence
status: Done
rfc: RFC-BPR-0007
depends_on: [BPR-0127, BPR-0126]
---

# Composition-root wiring and public-path evidence

Wire the ledger into the real application and prove the whole path through the
real executable rather than through a harness: a receipt marked in the running
application reaches SQLite and returns as an accepted projection, and survives
a restart of that application.

At most one application instance runs at a time.

## Required direct evidence

- The real executable marks a receipt, and the state is read back from the
  database file the application actually used.
- Restarting the real application shows the state persisted.
- The public-path markers report the ledger stage explicitly, so a regression
  names the stage rather than failing anonymously.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

`make_review_ledger_store` composes the durable ledger, and
`github_application` holds it beside the epoch catalog as a separate database
file — so deleting private review state cannot take the derived git cache with
it. Composition failure is deliberately **not** fatal: the ledger holds private
progress, and a reviewer with no ledger can still read a pull request.

### Evidence through the real executable

The probe commits a receipt through the store the *application composed* and
reads it back through that same store, so it reaches the database file the
application actually opened rather than one the probe chose. The public path
now requires `REVIEW_LEDGER_OK` in its expectation, which makes the evidence
load-bearing rather than merely printed.

### Restart is a new process, not a reopened handle

The epoch-maintenance contract already runs the real binary several times
against one application data root. The seed phase asserts
`REVIEW_LEDGER_PRESENT_ON_ENTRY:0` and the following reuse phase — a separate
execution of the binary — asserts `:1`. Reading before writing is what makes
the pair meaningful: without the 0, a probe that always reported "present"
would pass.

Two mistakes were made and corrected while wiring this:

The markers were first emitted between existing ones. The public-path
expectation matches its markers *contiguously*, so an unrelated line in the
middle broke a contract that has nothing to do with the ledger. They now follow
the sequence.

They were then emitted inside the render block, which the epoch phases skip
entirely — which is exactly where the restart evidence comes from. They are now
outside it.

Extra phases were also briefly inserted into the maintenance contract. That
contract is a stateful sequence (seed, prune, verify-pruned-delete), and extra
runs changed the state the later phases depend on. The assertions are folded
into the existing phases instead, and `run_phase` now checks a list of markers
rather than one.

### Non-vacuity

Two behaviours were broken separately and each failed, then restored to green:

- Skipping ledger composition — the public path no longer matches its
  expectation.
- The probe committing nothing — the maintenance contract reports "process
  omitted REVIEW_LEDGER_OK".

### Not covered here

The composed store is bound to a placeholder scope rather than the loaded pull
request's host, account, repository, and number. Binding it to the real
identity needs the ledger driven by the workbench, which is the UI-intent half
recorded on BPR-0127 and belongs with the remaining RFC-BPR-0007 work rather
than with composition.

The state directory is created by this adapter with an owner-only mode rather
than through the epoch catalog's hardened platform-specific bootstrap. The
adapter must not depend on the catalog having been composed first, so it
creates what it needs; reaching parity with that bootstrap is filed as
BPR-0132.
