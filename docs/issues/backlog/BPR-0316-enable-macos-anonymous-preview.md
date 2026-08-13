---
id: BPR-0316
title: Enable anonymous public preview on target-native macOS
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0306]
---

# Enable anonymous public preview on target-native macOS

## Required final state

- The exact target-native candidate course passes while production selection is
  unavailable, then this card flips only macOS public-preview selection in the
  same commit and reruns the complete isolated course against the enabled
  tests-disabled Release artifact.
- The course covers native HTTPS, zero Keychain/header, REST comments, rate/404,
  public Git smart HTTP, cache/epoch/diff, system browser/sign-in handoff, Cocoa
  presentation, runtime/dependencies, fixture absence, and hygiene.
- The one-time flip is provenance; BPR-0309 owns idempotent immutable final-head
  replay of already-enabled macOS after simplification/documentation.
