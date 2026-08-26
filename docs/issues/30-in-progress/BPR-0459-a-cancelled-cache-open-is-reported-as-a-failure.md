---
id: BPR-0459
title: A cancelled cache open is reported to the core as a failure
status: In Progress
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
