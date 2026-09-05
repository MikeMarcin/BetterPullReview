---
schema: "implement-rfc.card.v1"
id: "BPR-0397"
title: "Bound the three sibling actor waits that BPR-0390 left unbounded"
status: "Done"
rfcs: ["RFC-BPR-0003"]
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

## Done — 2026-08-19

All three waits now have a deadline and a stop callback, matching the
`materialize` precedent exactly: the callback is registered *before* the lock is
taken, sets `Abandoned` under the mutex, and notifies outside it.

| Function | Bound | Caller moved on | Actor never reached it |
|---|---|---|---|
| `resolve_selected_diff` | `BlobWait` | `cancelled` | `failure` |
| `materialize_agent_workspace` | `WorkspaceWait` | `cancelled` | `unavailable` |
| `cleanup_agent_workspace` | `WorkspaceWait` | `cancelled` | `cleanup_failed` |

`resolve_selected_diff` reuses `BlobWait` because it is the same class of work
as `materialize` — a read of objects the cache already holds. The two workspace
calls get their own `WorkspaceWait` (10 minutes, capped at 60) because
materializing writes a whole tree and cleanup removes one; a bound sized for a
read would fail honest work, which is a defect wearing a safety margin.

### The leak the bound would otherwise have created

Bounding `materialize_agent_workspace` introduced a problem the other two do not
have, and it is worth stating because the obvious repair is wrong. If the caller
gives up and the queued work *later succeeds*, it has written a workspace to
disk that nobody owns and nobody will clean up — the abandoned call is the only
thing that would have. So the closure now consults `Abandoned` under the waiter's
mutex and, if it holds a workspace, removes it before reporting `cancelled`. The
decision is made under the mutex on purpose: checking outside it leaves a window
where the caller abandons between the check and the store, which is the leak
again with extra steps.

Abandoning the other two costs nothing. A read's bytes are discarded; a
cleanup's removal still runs and only its outcome goes unread.

### The caller audit, and the one `{ }` that stays

The card required checking every caller for a token that can actually signal,
because BPR-0390's real defect was a `std::stop_token{ }` that made a wait *look*
cancellable. Three call sites, one per function:

- `workbench_application_host.cpp:638` passes the outstanding-request record's
  token, stopped by `retire`/`retire_all`. Signals.
- `agent_invocation_host.cpp:506` passes the invocation's token, stopped by
  `cancel`/`supersede`. Signals.
- `agent_invocation_host.cpp:472` passes a literal `{ }`. **It stays**, and this
  is the interesting one. Cleanup runs *because* the invocation ended, and
  `destroy` calls `cancel()` — which requests that very stop — immediately before
  calling here. Passing the invocation's token would abandon every shutdown
  cleanup and leak the workspace the call exists to remove. The token is
  deliberately inert; what makes this wait terminable is its bound. That is now
  said at the call site, because an undocumented `{ }` is exactly what took
  BPR-0390 three instrumented runs to see.

The four waits were **not** factored into one helper. They differ in return type,
in which limit bounds them, and in whether abandonment needs compensation, so a
common helper would take three parameters and a callback to express one shared
line. The card allowed this outcome explicitly: the bound matters, the
deduplication does not.

### Evidence

| Check | Result |
|---|---|
| `application_epoch_host`, `application_host`, `application_agent_invocation_host`, `application_uiforetw_selected_diff_contract`, `application_diff_workbench_public_path`, `application_epoch_persistence_github-com`, `application_epoch_maintenance_public_path` | 7/7 passed |
| `clang-format --dry-run --Werror` on all four changed sources | clean |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Three negative proofs were run, not assumed. Two required editing `src/` and were
run by the integrator:

| Negative proof | Result |
|---|---|
| `WorkspaceWait` clauses deleted from `valid(epoch_host_limits)` | fails: `a zero agent-workspace bound is rejected at composition` |
| `materialize_agent_workspace`'s `wait_for` reverted to `wait` | **`***Timeout 40.06 sec`** — the test hangs, which is the defect this card exists to remove |
| stopped caller token replaced by a stop-stateless `std::stop_token{ }`, expectations flipped to the bound's exits | passes in **30.45 s** instead of 0.41 s — three ten-second bounds, one per call |

That third one is the strongest of the three and it does double duty. With a
token that cannot signal, every call returns its *bound* exit after the full
bound; with a token that can, every call returns `cancelled` immediately. So
`cancelled` is produced by the stop token and by nothing else — and the same
three code paths released at 25 ms under small limits and at 10 s under large
ones, which is direct evidence that `BlobWait`/`WorkspaceWait` are what govern
the release rather than something incidental.

### Stated weaknesses

Neither committed case asserts a clock. Both discriminate by returned status
plus `queued_actor_work()` rising 1 → 2 → 3, which closes the real hole —
`materialize_agent_workspace` returns `unavailable` both for "no actor" and for
"bound elapsed", so without the count a mis-built request would pass vacuously.
But it means neither case would catch a regression that returned the right
status instantly for the wrong reason. Only the 30-second negative-proof run
demonstrates the timing, and that run is not in the committed test.

`workspace_parent_directory()` calls `std::filesystem::temp_directory_path()` in
a suite that otherwise uses only fake ports. Nothing is created — the fake cache
session answers without touching disk — but `valid()` requires an absolute
parent and there is no portable purely-lexical absolute path. Flagged rather
than hidden.

### Documentation

`docs/architecture/system-architecture.md` described `epoch_host_limits` as
capping only the job ledger and reentrant ingress. That was already incomplete
before this card — BPR-0390 added `BlobWait` without updating it — so the
paragraph now names both bounds, their defaults and maxima, and states that the
deadline is the backstop while the stop token is what makes an abandoned call
prompt.

### Full matrix — 2026-08-19

The focused selectors were the transition gate, but this change alters the
return values of three functions in a component most of the board composes, so
the wider run was warranted rather than ceremonial:

```
ctest --preset dev   ->   100% tests passed out of 193   (1546 s)
```

Nothing regressed, and nothing red pre-existed at this commit.
