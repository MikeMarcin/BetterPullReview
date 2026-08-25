---
id: BPR-0282
title: Verify device-authorization security and public behavior
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0281, BPR-0450]
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

## Inherited from done BPR-0450 — 2026-08-25

The refresh wire contract and its production adapter are in place, and the
selector this card's ledger row names — `github_device_authorization_refresh_contract`
— is registered and green with eight loopback scenarios. Two facts this card
must not restate more strongly than they are:

- The renewal wire shapes are `documented_not_recorded`, filed in
  `tests/fixtures/github/device-authorization/documented-v1/` deliberately
  outside BPR-0276's digest-bound recorded tree. No recorded-GitHub claim is
  admissible for refresh, and `capture_rfc0016_device_contracts.py` remains
  structurally unable to emit one.
- The production `device_authorization_transport_port` implementation
  (`github_device_authorization_transport`) ships here rather than in
  [[BPR-0281]], because a partial port could not exist. BPR-0281 keeps the
  composition wiring, the timer adapter, and per-platform selection.
