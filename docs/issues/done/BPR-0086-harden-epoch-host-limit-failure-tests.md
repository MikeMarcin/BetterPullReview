---
id: BPR-0086
title: Harden epoch host limit and allocation-failure tests
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0068]
---

# Harden epoch host limit and allocation-failure tests

Add a deterministic allocation-failure seam around epoch-host admission and
exercise rollback at each recoverable allocation boundary. Extend the public
limit-validation table with zero deferred-ingress capacity and values above
both hard maxima. Require exact failure diagnostics, zero partial admission,
lease revocation, unchanged active authority, and shutdown quiescence.

This is nonblocking hardening identified by the BPR-0068 closeout review. The
current implementation has direct source review and executor-refusal evidence
for allocation-safe rollback plus positive and job-limit boundary coverage;
this card does not reopen BPR-0068 or block RFC-BPR-0003 closeout.

## Implementation

The seam follows the existing SQLite convention rather than inventing a second
pattern. `BUILD_TESTING` compiles `BPR_APPLICATION_TESTING` into the
application library, and the new
`src/application/include/bpr/application/testing.hpp` declares
`bpr::application::detail::testing::fail_next_epoch_job_admission` together
with `clear_epoch_job_admission_failure`.

`admit_job` has three recoverable allocation boundaries, and the seam names
each one: the `job_control` allocation, the `decision_task` type-erasure
allocation, and the `job_ledger` growth. Arming a stage makes the next
admission reaching it throw `std::bad_alloc`, which the function's existing
catch converts to `unavailable`. The arming is consumed by a
compare-and-exchange, so exactly one admission is affected and a stale arming
cannot leak into unrelated later work.

No production behavior changed. The seam adds three guarded call sites and no
unguarded code, and the rollback it exercises is the one that already existed.

## Direct verification

- `application_epoch_host` passes. It now drives all three admission
  boundaries to fail and requires, at each one, no partial job, actor entry, or
  scheduled effect; lease revocation for the rejected offer with the active
  authority unchanged; exactly one `unavailable` diagnostic; and shutdown
  quiescence afterwards. A further case proves one arming is consumed exactly
  once and the following admission proceeds normally.
- The new case was confirmed not to be vacuous. Replacing the seam's `throw`
  with a no-op makes `application_epoch_host` fail on the zero-partial-admission
  assertion; restoring it passes.
- The public limit-validation table now rejects zero deferred-ingress capacity,
  a job capacity above `maximum_epoch_outstanding_jobs`, and a deferred-ingress
  capacity above `maximum_epoch_deferred_ingress`, while still admitting both
  hard maxima themselves.
- `application_composition_contract` passes in 37.27 seconds with
  `fail_next_epoch_job_admission` and `clear_epoch_job_admission_failure` added
  to the tests-disabled Release forbidden-byte list, so the seam provably
  cannot reach a production binary. The same two symbols are present in the
  tests-enabled static library, which confirms the guarded strings are the
  right ones to forbid rather than a pair that could never appear.
- The 181-file format gate, the strict first-party `clang-tidy` audit, and
  `DOCUMENTATION_OK` pass.

## Documentation disposition

The fixture catalog records the seam, its three boundaries, the
single-consumption rule, and the Release forbidden-byte binding. The
RFC-BPR-0003 verification ledger's bounded-resource claim and performance plan
record that the allocation boundaries now carry executed failure evidence while
the repository-actor, prepared-observation, recovery-scan, and identity-retry
bounds remain enforcement-site evidence.

## Result

Each recoverable admission allocation boundary now has direct, non-vacuous
failure evidence instead of source review alone, and the limit-validation table
covers both hard maxima and both zero cases. The seam is test-only by
construction and by an enforced Release contract. BPR-0086 is complete.
