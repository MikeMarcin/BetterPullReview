---
id: BPR-0548
title: The fixture vault obeys the platform vaults' rules
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The fixture vault obeys the platform vaults' rules

## Why

The three blocking findings of RFC-BPR-0016's third review ([[BPR-0285]],
[[BPR-0545]], [[BPR-0546]], [[BPR-0547]]) were all invisible to every
registered row for one reason: the fixture vault every composed course runs
against does not share the platform vaults' rules. It does not key pending
operations by context, so it accepts two operations under one context where
every real vault refuses; it does not validate refresh-slot contexts, so it
accepts a context whose identity generation is zero. The host tests' double
has the same two gaps.

A fixture that is more permissive than the thing it stands in for cannot
falsify anything about that thing. This card closes the class rather than
the three instances.

## Required final state

- The fixture vault and the host tests' vault double enforce what every
  platform vault enforces: `valid()` on every context and enrollment for
  both slots, one pending operation per context with a typed
  `invalid_request` for a duplicate, and the same completion discipline.
- A short list in the fixture's own comment states which platform rules it
  mirrors and why each one is there, so the next divergence is deliberate.
- Applying this before the three fixes turns the affected courses red;
  those runs are recorded as the negative controls for
  [[BPR-0545]] through [[BPR-0547]].
- No composed course loses coverage: a course that now refuses is repaired
  by fixing the composition, never by relaxing the fixture.

## Non-goals

- Making the fixture a second implementation of a platform vault; it stands
  in for one, and only its refusals must match.
