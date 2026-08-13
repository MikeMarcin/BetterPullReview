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
- Source, binary, fixture, log, settings, SQLite, process, and environment scans
  find no client secret, access/refresh token, device code, or user code.
- A registered-contract public course proves one vault enrollment and no
  settings/SQLite copy without contacting or mutating an undesignated target.
