# RFC-BPR-0001 Foundation Implementation

This document describes the live foundation selected by the executable. The
broader normative design remains in [System Architecture](system-architecture.md).

## Selected vertical slice

```text
Refresh QAction
  -> refresh_requested_intent
  -> foundation_observation
  -> core::transition
  -> refresh_foundation_request(request, session, generation)
  -> local foundation effect
  -> queued foundation_refreshed_event
  -> core::transition
  -> command/status deltas
  -> foundation_window incremental projection
```

The public `better_pull_review --self-test` command triggers this exact path.
It succeeds only after the action is disabled while pending, the completion
returns through the Qt event-loop executor, the action is re-enabled, the
status becomes current, and no full reset occurs.

## State authority

- GitHub shared facts are modeled separately from local private facts.
- `resolve_shared_revision` always selects the GitHub revision for a shared
  disagreement.
- The foundation stores only compact session, request, generation, pending
  request, and shared revision state.
- The current foundation effect is deliberately local and performs no network,
  Git, database, credential, or process operation. Those arrive behind the
  established ports in dependent RFCs.

## Asynchrony and lifecycle

Effect callbacks post typed events to `event_loop_executor`; they cannot mutate
the core or window directly. The core accepts a completion only when request,
application session, and generation match the pending work. Shutdown closes
ingress, cancels pending work by request identity, and invalidates the
completion gate before host destruction.

Construction order is observations, effect, decision executor, Qt window, then
host. Destruction is the reverse, so the host cancels while its dependencies
and projection remain alive.

## Current deliberate limits

The foundation renders an empty state and proves architecture flow. It does not
fetch a pull request, authenticate to GitHub, persist a ledger, compute a diff,
or package a release. Those are not hidden stubs; they are dependent RFCs.
