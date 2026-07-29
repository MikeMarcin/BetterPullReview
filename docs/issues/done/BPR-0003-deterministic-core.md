---
id: BPR-0003
title: Implement the deterministic functional-core vertical slice
status: Done
rfc: RFC-BPR-0001
depends_on: [BPR-0002]
completed: 2026-07-29
---

# Implement the deterministic functional-core vertical slice

Implement compact state, typed intent/event/observation/request/delta records,
request identity and generation, command legality, diagnostics, deterministic
transitions, replay tests, and product-authority tests without Qt or effects.

## Result

- Implemented deterministic refresh intent and completion transitions.
- Added monotonically generated request and refresh-generation identities.
- Rejected mismatched request, application-session, and generation completions
  without changing current state.
- Kept GitHub shared, local private, and derived authority categories explicit;
  local private revision facts cannot override the GitHub shared revision.
- Emitted typed host requests, command/status deltas, changed-domain masks, and
  reason-coded diagnostics.
- Kept `application_state` at or below the directly tested 64-byte bound.

## Evidence

`core_foundation` covers request creation, authority resolution, semantic
rejection, every asynchronous identity dimension, accepted completion, and
identical two-pass event/observation replay. It passes in both `core-only` and
Qt-enabled `dev` presets alongside `core_dependency_boundary`.
