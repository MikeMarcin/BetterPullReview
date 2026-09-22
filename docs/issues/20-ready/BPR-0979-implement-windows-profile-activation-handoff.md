---
schema: "implement-rfc.card.v1"
id: "BPR-0979"
title: "Implement bounded Windows profile activation and New Window handoff"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0710", "BPR-0711", "BPR-0813", "BPR-0932"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Implement bounded Windows profile activation and New Window handoff

## Scope

Complete RFC-0024 AC-06/09's native Windows single-owner launch contract.
Keep the existing exclusive profile runtime lease. Add a same-user local,
bounded activate/New Window handoff to that owner; never transport credentials
or arbitrary commands. Validate identity, request size/version, timeouts,
endpoint ownership and lifecycle; distinct profiles remain independent.
Preserve the existing POSIX short-rendezvous implementation and evidence.

Owned surfaces: profile activation adapter/contract and native Windows
implementation, minimal composition wiring if needed, adapter/public-path
tests and native candidate evidence. Never edit live station checkouts.

## Acceptance

- Native Windows repeated ordinary launch activates the owner profile's MRU
  window; explicit New Window creates another window without a second owner.
- Concurrent profiles and owner shutdown/restart remain isolated; malformed,
  cross-profile, untrusted endpoint and timed-out requests fail closed within
  explicit bounds, with no credential or arbitrary-command transport.
- Focused native Windows adapter/public tests and POSIX regressions pass.
  Unrun native acceptance remains pending and cannot be inferred from compile
  success or another platform.

## Evidence

Pending. The current Windows adapter returns unavailable; runtime leases alone
do not satisfy activation or explicit New Window.
