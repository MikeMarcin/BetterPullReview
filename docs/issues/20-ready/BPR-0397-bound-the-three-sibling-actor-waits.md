---
id: BPR-0397
title: Bound the three sibling actor waits that BPR-0390 left unbounded
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# Bound the three sibling actor waits that BPR-0390 left unbounded

[[BPR-0390]] found that `epoch_application_host`'s caller side of `materialize`
waited on a condition variable with no timeout and no usable stop token, and
that this is what left a classification outstanding for an entire session while
its pane rendered monochrome. It repaired that one wait and deliberately left
its three siblings alone, because bounding them changes the behaviour of paths
that card made no claim about.

They are still there, and they are the identical shape:

| Function | Declared | Wait |
|---|---:|---:|
| `resolve_selected_diff` | `epoch_application_host.cpp:2996` | `:3325` |
| `materialize_agent_workspace` | `:3532` | `:3694` |
| `cleanup_agent_workspace` | `:3700` | `:3811` |

Each is `waiter->Completed.wait(lock, ...)` with no deadline, while the repaired
`materialize` at `:3515` is now `wait_for` against `epoch_host_limits::BlobWait`
with a real `std::stop_source`.

Two things make this worth its own card rather than a cleanup note.

**The failure is not hypothetical.** The enqueued work is FIFO behind whatever
the repository actor is already running, and epoch maintenance can hold that
actor for most of a minute. BPR-0390 measured a wait that only ended when
shutdown's `BlobIngressClosing` released it. The same release is the only exit
these three have.

**A default-constructed stop token is worse than no token.** BPR-0390 found
`composition_blob_source` passing `std::stop_token{}` — a token with no stop
state, which can never signal — so the caller looked cancellable and was not.
Any repair here has to check what each caller actually passes, not that a
parameter named `aStop` exists.

## Required final state

- Each of the three waits has a bound and an honest cancellation path, with the
  two exits distinguished the way `materialize` distinguishes them: `cancelled`
  for a caller that has moved on, a failure status for an elapsed bound.
- Each bound is validated at composition, non-zero, and capped, following
  `epoch_host_limits::BlobWait`'s precedent. A bound chosen tight enough to fail
  a legitimate read is a defect, not a safety margin; promptness is the stop
  token's job.
- Every caller is checked for a token that can actually signal. A caller passing
  a stop-stateless token is part of this card, not a later surprise.
- The four waits are factored into one helper if that can be done without
  changing any of the four behaviours. If it cannot, say so and leave them
  separate — the bound matters and the deduplication does not.
- A test drives each of the three to its bound and to its cancellation, and
  fails if either exit is removed. `application_epoch_host`'s three BPR-0390
  limit cases are the shape to follow.

## Non-goals

- Changing what `resolve_selected_diff` returns to the diff pipeline, or what
  the agent workspace paths report to the operator, beyond adding the two new
  exits.
- Revisiting `materialize`'s repaired wait or its 90-second default.
- [[BPR-0348]]'s separate finding that the git subprocess adapter discards the
  error text these paths would want to report.

## Verification

```sh
ctest --preset dev -R '^(application_epoch_host|application_epoch_maintenance_public_path|application_epoch_persistence_github-com|application_uiforetw_selected_diff_contract)$' --output-on-failure
```

Plus, for each of the three bounds, the negative proof BPR-0390 used: delete the
validation, re-run, require the composition to reject it, restore.
