---
id: BPR-0004
title: Implement the application host and manual executor
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0003]
completed: 2026-07-29
---

# Implement the application host and manual executor

Implement narrow capability ports, request dispatch, a serialized decision
lane, deterministic manual execution, completion events, stale-result
rejection, cancellation, diagnostics, and explicit shutdown behavior.

## Result

- Replaced the application placeholder with a concrete host library.
- Added narrow observation, foundation-effect, decision-executor, and
  projection ports rather than a universal service object.
- Applied core decisions from bounded observations and executed only typed
  requests.
- Marshalled effect completions onto an injected serialized decision lane;
  callbacks cannot mutate core state or publish projections directly.
- Added a deterministic manual executor that can run tasks in any selected
  order.
- Added cancellation and closed-ingress behavior during explicit shutdown.
- Added a weak completion gate so queued completions cannot dispatch through a
  destroyed host registration.

## Evidence

`application_host` directly verifies delayed completion, arbitrary task
reordering, duplicate semantic intent, a late earlier generation, accepted
current generation, shutdown cancellation, and post-shutdown suppression. The
suite passes in both `core-only` and Qt-enabled `dev` presets.
