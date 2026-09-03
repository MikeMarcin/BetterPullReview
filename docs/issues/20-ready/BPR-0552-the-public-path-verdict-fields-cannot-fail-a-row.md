---
id: BPR-0552
title: The public-path verdict fields cannot fail a row that does not pin PUBLIC_PATH_OK
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The public-path verdict fields cannot fail a row that does not pin PUBLIC_PATH_OK

## Why

Found while closing [[BPR-0551]], which fixed one instance of this. The whole
`PUBLIC_PATH_STAGE` / `VERDICT` / `GROUPS` / `BUDGET` / `STATUS` /
`DIAGNOSTIC` / `ACCOUNT` / `CONVERSATIONS` / `EPOCH_*` block in
`src/app/main.cpp` is printed inside `if (!passed)`, and none of those names
appears in any `PASS_REGULAR_EXPRESSION` under `tests/`. So for every row that
does not pin `PUBLIC_PATH_OK` — and neither device course does — every field
of `PUBLIC_PATH_VERDICT` is unfailable: not only `cleanup_failed`, which
[[BPR-0551]] gave its own always-emitted marker, but `presentation`,
`shutdown_complete`, `epoch_projection_failed`, and `epoch_handoff_failed`.
A regression in any of them leaves those rows green.

Adding `PUBLIC_PATH_OK` as a second entry in a row's expression list does not
fix it: CTest passes a test when any listed expression matches, so the
weaker one carries the row.

Ten rows do pin `PUBLIC_PATH_OK` and are unaffected. The exposure is the
rows that pin only their own domain's markers, which is most of the
device-authorization and public-preview family.

## Required final state

- Every row's verdict is decided by markers the run always emits, not by a
  block that appears only after the run has already failed. Either the
  verdict fields move into an always-emitted block per domain, or each
  affected row gains an explicit failure expression, or the runner fails the
  row on the child's exit code — chosen with a recorded reason and applied
  uniformly rather than row by row.
- A deliberate regression of one verdict field turns at least one row of each
  affected family red; those runs are the controls.
- Any card or ledger row whose claim rests on one of these fields is
  corrected or restated in the same slice.

## Non-goals

- Changing what any course proves about its own domain.
- Re-litigating [[BPR-0551]]'s cleanup marker, which stands.
