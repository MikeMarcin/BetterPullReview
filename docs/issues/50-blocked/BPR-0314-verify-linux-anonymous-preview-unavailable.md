---
id: BPR-0314
title: Verify anonymous preview remains unavailable on Linux
status: Blocked
rfc: RFC-BPR-0017
depends_on: [BPR-0306]
blocked_by: platform-linux
---

# Verify anonymous preview remains unavailable on Linux

## Required final state

- A clean supported Linux target builds the tests-disabled Release application
  and selects typed public-preview unavailability for GitHub.com and GHES.
- Exact counters prove zero vault/keyring, Authorization header, REST/GraphQL,
  Git smart HTTP, browser, mutation, child-process, and private-content effects.
- Runtime/dependency/ELF, Qt presentation, fixture-seam absence, documentation,
  and hygiene gates pass target-native. No enablement claim is made.
- This card's first target-native course is provenance for the unavailable
  selection; BPR-0309 reruns the same idempotent course at immutable final head
  after cross-platform simplification and documentation reconciliation.
