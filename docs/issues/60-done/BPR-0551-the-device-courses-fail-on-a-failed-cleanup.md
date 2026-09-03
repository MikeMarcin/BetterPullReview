---
id: BPR-0551
title: The device courses fail on a failed shutdown cleanup
status: Done
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

## Landed — 2026-09-03

At `45ad5e1e`. Both device courses now print
`DEVICE_FLOW_CLEANUP_FAILED:<true|false>` unconditionally inside their own
marker block — the refresh course after its stored-token line, the registered
course inside its identity block, above the counts the browser and panel rows
read as adjacent lines — and both rows pin it `false`.

**The control is decisive.** With the erasure sequencing deliberately
re-regressed (the refresh delete moved back beside the credential delete,
under one context), both rows turn red:

```
application_device_authorization_registered_course  ***Failed
  DEVICE_FLOW_CLEANUP_FAILED:true
application_device_authorization_refresh_course     ***Failed
  DEVICE_FLOW_ROTATIONS:1  DEVICE_FLOW_REFRESH_STATE:reenrollment_required
  DEVICE_FLOW_CLEANUP_FAILED:true  DEVICE_FLOW_REFRESH_FAILED
```

The registered course is the one that settles it: every other field it pins
is byte-identical to its old values, so that row stayed green under this
regression before this slice and now fails on the marker alone. The
regression was reverted and the file confirmed unmodified.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A failed shutdown cleanup fails a composed row | both device courses with the erasure re-regressed | both red, on the marker alone for the registered course | this commit |
| The marker is false in the shipped path | both device courses at this head | `DEVICE_FLOW_CLEANUP_FAILED:false`, 16/16 in the selector set | this commit |

**What this found beyond its own scope.** The gap was not confined to one
field. The entire `PUBLIC_PATH_*` diagnostic block is printed only after a
run has already failed, and none of its names appears in any row's pass
expression, so for any row that does not pin `PUBLIC_PATH_OK` every field of
`PUBLIC_PATH_VERDICT` is unfailable — `presentation`, `shutdown_complete`,
and both epoch fields as well as `cleanup_failed`. Adding `PUBLIC_PATH_OK` as
a second expression would not help, because CTest passes a row when any
listed expression matches. Carded as [[BPR-0552]].

That card is **not** in RFC-BPR-0016's closeout closure, and the reason is
recorded rather than assumed: the fields still unguarded are the review
surface's and the epoch host's, not this RFC's. RFC-BPR-0016's acceptance
rows rest on the `DEVICE_FLOW_*` markers, which the rows do pin, and on
cleanup, which this card just made failable. Widening the closure to a
general harness-hygiene defect would hold this RFC for work it does not
depend on.
