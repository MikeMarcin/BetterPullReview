---
id: BPR-0552
title: The public-path verdict fields cannot fail a row that does not pin PUBLIC_PATH_OK
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The public-path verdict fields cannot fail a row that does not pin PUBLIC_PATH_OK

## Why

Found while closing [[BPR-0551]], which fixed one instance of this. The whole
`PUBLIC_PATH_STAGE` / `VERDICT` / `GROUPS` / `BUDGET` / `STATUS` /
`DIAGNOSTIC` / `ACCOUNT` / `CONVERSATIONS` / `EPOCH_*` block in
`src/app/main.cpp` is printed inside `if (!passed)`, and none of those names
appears in any `PASS_REGULAR_EXPRESSION` under `tests/` — **wrong by exactly
one row**, corrected at close: `application_automated_run_budget` pins
`PUBLIC_PATH_STAGE:2`, `PUBLIC_PATH_VERDICT:`, `PUBLIC_PATH_GROUPS:`, and
`PUBLIC_PATH_BUDGET:`, because a failed run is its subject. So for every other row that
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

## Landed — 2026-09-04

At `60081ccc`. **One uniform `FAIL_REGULAR_EXPRESSION "PUBLIC_PATH_FAILED"`,
set by `bpr_add_network_isolated_test`**, with an `EXPECTS_FAILED_RUN`
opt-out for the one row whose subject is a failed run.

`report_verdict` prints `PUBLIC_PATH_OK` or `PUBLIC_PATH_FAILED`
unconditionally, from the same conjunction that produces the process's exit
status, so the marker *is* the exit code expressed where CTest can read it,
and it negates all five verdict fields at once rather than one marker per
field.

**Why not the other two, checked against the code rather than assumed.** A
second pass expression cannot work: CTest passes a row when any required
expression matches, so the weaker one carries it. Failing on the child's exit
code is unreachable while a pass expression exists at all —
`cmake` computes success as `retVal == 0 || !RequiredRegularExpressions.empty()`,
measured directly on this CMake in a two-test scratch project: a test printing
its marker and exiting 1 passed with a pass expression, and failed only once a
matching failure expression was added. The isolated runner already returned the
child's code unchanged; nothing downstream was looking at it. Reaching that
route would have meant deleting the marker pinning from forty rows.

Setting it at the registration seam matters: that function is the single entry
for all 275 isolated rows on all three platforms, so a new row inherits the
rule and cannot drift out of it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A regressed verdict field turns the affected family red | `epoch_handoff_failed` deliberately inverted, same binary both runs | with the failure expression, 0 of 7 passed, each on `Error regular expression found in output`; with it disabled by reconfigure only, 5 of 7 passed on exactly the same regression | this commit |
| The two rows that already pinned `PUBLIC_PATH_OK` were red either way | the same pair of runs | red in both, as expected — they were never the exposure | this commit |
| The regression is gone | `grep -c` on the restored conjunct | 1 | this commit |
| Every affected row and the opt-out row | `ctest --preset dev -j 2` over the ten-row set | 10/10 | this commit |

**Left open.** Rows whose driver script captures the child's output rather
than letting CTest see it are decided by their own result check, which those
scripts already make; they are unaffected by this rule and unchanged.

