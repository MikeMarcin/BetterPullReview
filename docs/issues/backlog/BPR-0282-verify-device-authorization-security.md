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
- After every macOS target-native row passes, this card owns the production
  macOS capability-selection flip in the same commit, followed by a clean
  Release/public/native/hygiene rerun proving the enabled artifact. Failure of
  any row leaves macOS unavailable and the card nonterminal.
- The one-time macOS flip record is provenance. BPR-0285 separately reruns the
  exact idempotent final-head platform course against already-enabled macOS
  after simplification, documentation, and every required card are terminal.
