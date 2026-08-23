---
id: BPR-0427
title: The epoch catalog hangs inside an AppContainer instead of failing
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# The epoch catalog hangs inside an AppContainer instead of failing

Inside the AppContainer, with the ancestor chain unreadable ([[BPR-0362]]),
the two epoch-catalog tests do not fail — they stop producing output and
CTest kills them:

| test | contained | uncontained |
|---|---|---|
| `sqlite_epoch_catalog_storage_contract` | **Timeout at 30 s, no output** | fails later, at a retention assertion ([[BPR-0425]]) |
| `sqlite_epoch_catalog_behavior_contract` | **Timeout at 180 s, no output** | passes, in 99.5 s |

The two sibling stores that open a state directory through the same
`open_state_directory` path fail *fast* under the same denial —
`sqlite_review_ledger_storage` with `the bounded state directory opens`,
`sqlite_review_mutation_recovery_storage` with
`owner-private recovery store opens` — so the denial itself is not what hangs.
Something between the catalog's first open and its first diagnostic retries or
waits when the chain cannot be walked. [[BPR-0362]] observed the same shape on
2026-08-18 ("no output at all for seven minutes") and later recorded that the
tests had begun to fail fast; on this tree, at `a74e1df`, they hang again.

This is not only a test-suite matter. [[BPR-0360]] records why: an
MSIX-packaged application runs inside an AppContainer, and a catalog that
hangs at startup rather than reporting `invalid_permissions` is a worse
failure for a real user than one that fails closed.

## Required final state

- The wait is located: which call blocks, what it is waiting for, and why the
  ledger and recovery stores on the same path do not.
- With the chain unreadable, the catalog fails closed with a typed diagnostic
  in bounded time, contained, the way its siblings do.
- A test fails without the change — necessarily a contained one, which means
  a deliberately registered contained probe rather than the two catalog
  tests, which [[BPR-0362]] registers `WINDOWS_UNCONTAINED`.
- Uncontained behaviour, and macOS and Linux behaviour, are unchanged.

## Verification

A contained probe through the launcher against a catalog root whose chain
the container cannot read, asserting a typed failure within the CTest timeout;
the exact registration is this card's to design.

```sh
ctest --preset core-only --output-on-failure -R '^sqlite_epoch_catalog_(storage|behavior)_contract$'
```
