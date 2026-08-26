---
id: BPR-0459
title: A cancelled cache open is reported to the core as a failure
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# A cancelled cache open is reported to the core as a failure

`start_comparison_resolution` distinguishes cancellation from failure on every
catalog result and on no cache result. Its two catalog calls each check
`epoch_catalog_failure::cancelled` and dispatch
`core::comparison_resolve_cancelled_event`
(`src/application/src/epoch_application_host.cpp:6076` and `:6144`). The cache
open three lines later does not:

```cpp
const auto openedCache = open_actor_cache(*context->Actor, *caches, expectedCache, aStop);
if (!openedCache) {
    const auto failure = cache_failure(openedCache.error());
    return [aRequest, failure](implementation& aHost) {
        aHost.dispatch_core(core::comparison_resolve_failed_event{ ... });
    };
}
```

`cache_failure` maps `git_cache_failure::cancelled` to
`core::epoch_failure::unavailable` (`:317`), so a stop token that was requested
while `git_cache_port::open` was in flight reaches the core as
`comparison_resolve_failed_event{ unavailable }`. The core then takes
`epoch_status::failed` with `reason_for(unavailable)` where
`epoch_status::cancelled` with `epoch_reason_code::cancelled` is the truth
(`src/core/src/epoch_transition.cpp:1512` and `:1528`).

## How it was found, and what is not claimed

Found while diagnosing [[BPR-0447]]. That card's failing runs emit
`EPOCH_CACHE_FAILURE:14:` followed by `EPOCH_EFFECT_FAILED:comparison-resolve:0`
â€” a cancelled cache open arriving at the core as `unavailable`.

**It is not BPR-0447's cause and repairing it would not have fixed that card.**
In those runs the epoch selectors still read `Resolving the exact comparisonâ€¦`
at the moment the run gave up, so the failed event was rejected by
`comparison_context_match` as stale and changed no state; BPR-0447's repair is
in its fixture course and is verified without touching this. What is unproven
here is the reverse case â€” a cancelled cache open whose context *does* still
match â€” which is the case that would show a user `unavailable` for work that
was cancelled.

## Required final state

- A cancelled cache open in `start_comparison_resolution` reaches the core as
  `comparison_resolve_cancelled_event`, or the reason it must not is recorded.
- The same question is answered for every other `cache_failure` call site in
  `epoch_application_host.cpp`, since they all share the one `cancelled` â†’
  `unavailable` mapping. Sites that deliberately want `unavailable` say so.
- A core or host test fails if a cancelled cache result produces
  `epoch_status::failed`. Without one this is a mapping nothing checks.

## Non-goals

- Not BPR-0447's stage-14 wait, which is committed and verified separately.
- Not a change to `cache_failure`'s other mappings.

## Closed — 2026-08-26

Fixed at `5cb734d4`. The comparison-resolve cache open now carries the
same cancelled guard as its two catalog siblings and dispatches
`comparison_resolve_cancelled_event`; the audit found one more site of the
same class — `start_catalog_commit`'s append-branch cache open used
`abortAndFail` unconditionally three lines above the `abortAndCancel` its
own stop check already used — and it now takes the same distinction.

Every other `cache_failure` call site is answered as the card required.
The four `start_revision_preparation` sites already guarded cancelled.
The recovery and diagnostic sites (`recover_append`, `recover_maintenance`,
`recover_catalog`, and the post-commit `visible_cleanup_pending`
bookkeeping) keep `unavailable` deliberately: their pipeline's
`core::epoch_failure` has no cancelled member, they feed
`publish_diagnostic` or non-cancellable stop tokens rather than an
epoch-status transition, and the cleanup site must never replace a
committed terminal — so the mapping cannot misreport a state transition
there.

The reverse case the card called unproven is now a failing-if-wrong test,
twice. A host-level stop request also moves the comparison context, so the
suite injects the cancellation instead: a canned `CacheOpenFailures` entry
for the resolve's only open, and a foreign retained session — the state a
repository actor really produces when it switches cache sessions between
pull requests — so the commit must reopen. Both tests first assert the
exact `cache.open` count, then require `epoch_status::cancelled`, a
`cancelled` diagnostic, and no `unavailable` one.

Evidence, all target-native on the Windows host: `application_epoch_host`
passed at `f8799ca9`; with the two source hunks reverted to `17063ffb`'s
mapping the suite fails at exactly
`FAILED: a cancelled cache open leaves the comparison cancelled rather
than failed`, and passes again restored. The repository `clang-format`
(22.1.3) check passed on both touched files. One behavioral note for
future log readers: a cancelled open at these two sites no longer prints
the testing-build `EPOCH_CACHE_FAILURE:14:` breadcrumb [[BPR-0447]]'s
diagnosis leaned on, because the guard now short-circuits before
`cache_failure` — the same shape the prepare-path sites already had.

Implemented by rfc-worker from an rfc-scout call-site map; reviewed,
verified, and closed by the integrator.
