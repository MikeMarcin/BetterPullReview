---
id: BPR-0551
title: The device courses fail on a failed shutdown cleanup
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The device courses fail on a failed shutdown cleanup

## Why

The fourth independent review of RFC-BPR-0016 at `1f1f986c` ([[BPR-0285]])
found that [[BPR-0545]]'s composed evidence cannot fail. Its card claims the
refresh course "completes its cleanup — `cleanup_failed=0` where the control
had `1`", but `PUBLIC_PATH_VERDICT`, which carries that field, is printed
only when the run has already failed for another reason; the field is not in
the row's `PASS_REGULAR_EXPRESSION`; `DEVICE_FLOW_REFRESH_OK` does not
consult the host's `shutdown_cleanup_failed()`; no `FAIL_REGULAR_EXPRESSION`
is registered; and the isolated runner passes the child's exit code through
to CTest, which does not fail a test on exit code once a required expression
matches.

So a re-regression of the third review's B1 — the refresh erasure displacing
the credential delete, leaving the access token in the vault behind a failed
cleanup — would leave both device course rows green. The hermetic
`test_an_occasion_never_has_two_operations_pending_under_one_context` is
today the only guard, and the composed path has none.

## Required final state

- Both device courses print the cleanup verdict inside their own
  `DEVICE_FLOW_*` block, as a marker that is always emitted rather than one
  that appears only on failure, and pin it.
- A deliberate re-regression of the erasure sequencing turns at least one
  composed row red; that run is recorded as the control.
- Any other composed claim on a card that rests on a field the row cannot
  fail on is corrected in the same slice, or restated as what it is.

## Non-goals

- Changing what the courses prove about rotation, retention, or provenance.
