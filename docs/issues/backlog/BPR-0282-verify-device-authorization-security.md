---
id: BPR-0282
title: Verify device-authorization security and public behavior
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0281]
---

# Verify device-authorization security and public behavior

## Required final state

- Focused core, host, adapter, native HTTPS, Qt, composition, tests-disabled
  Release, sanitizer, runtime, dependency, and boundary matrices pass locally.
- Hostile courses prove interval/backoff/total bounds, cancellation/shutdown,
  malformed/oversized bodies, off-authority addresses, both endpoint redirect
  and certificate failures, browser failure, and zero-request unavailability.
- For a selected GitHub App, controlled-clock/native-HTTPS/vault courses prove
  refresh before expiry, atomic access/refresh rotation, stale/canceled refresh
  rejection, refresh-material cleanup, and typed recoverable re-enrollment on
  refresh failure.
- Source, binary, fixture, log, settings, SQLite, process, and environment scans
  find no client secret, access/refresh token, device code, or user code.
- A registered-contract public course proves one vault enrollment and no
  settings/SQLite copy without contacting or mutating an undesignated target.
- Target-native evidence covers every platform on which device flow is claimed
  available. Untested platforms remain explicitly unavailable or retain an
  open platform-owned card; macOS, cross-compilation, mocks, and hosted
  workflows cannot prove Windows or Linux behavior.
- The complete macOS target-native course runs against production selection
  still unavailable and proves the owner-attended implementation without
  treating the private, owner-only verification app as a distributable client.
  Failure of any row leaves this card nonterminal.
- BPR-0318 owns the later distributable identity, one-time macOS capability-
  selection flip, and clean enabled rerun. BPR-0285 separately reruns the exact
  idempotent final-head platform course after simplification, documentation,
  and every required card are terminal.
