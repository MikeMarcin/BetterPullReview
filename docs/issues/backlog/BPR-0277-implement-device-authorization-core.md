---
id: BPR-0277
title: Implement deterministic device-authorization policy
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0276]
---

# Implement deterministic device-authorization policy

## Required final state

- Core owns compact typed states, intents, revision/request identity, semantic
  legality, stale-result rejection, terminal diagnostics, and cancellation.
- Poll timing, interval, backoff, and transport retry mechanics remain host
  effects owned by BPR-0279 and never enter core policy.
- Core and projection types cannot represent client identifiers, endpoints,
  poll intervals, device/user codes, access tokens, or refresh tokens.
- Replay/property tests cover every legal and illegal transition, duplicate and
  stale observations, cancellation, denial, expiry, unavailable, and success.
- No Qt, provider, filesystem, process, clock, timer, thread, network, database,
  credential, or hidden-global dependency enters `src/core/`.
