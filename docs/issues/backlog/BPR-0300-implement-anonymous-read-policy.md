---
id: BPR-0300
title: Implement deterministic anonymous-read policy
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0299]
---

# Implement deterministic anonymous-read policy

## Required final state

- Core owns explicit public-preview mode, stable request/revision identity,
  legal read-only intents, unavailable viewer/thread-resolution/viewed/action
  observations, rate/private/GHES reasons, and authenticated-switch policy.
- No provider, URL, authorization, vault, account credential, clock, thread,
  filesystem, Qt, database, or default synthetic viewer enters core.
- Replay/property tests cover every legal/illegal action, stale/duplicate
  observation, switch, refusal, rate terminal, and missing capability.
