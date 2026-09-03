---
id: BPR-0316
title: Enable anonymous public preview on target-native macOS
status: In Progress
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

## Authorized and claimed — 2026-09-02

[[BPR-0306]] is terminal. The product owner authorized the native candidate
proof this card requires — in full: "authorized" — against the same public
pull request the RFC-BPR-0017 probe captures came from,
`octocat/Hello-World#1`. Reading that pull request through the application's
own anonymous provider issues one REST request per eligible operation —
nine in all, each unauthenticated, each spending one unit of the shared
hourly core budget — and no GraphQL, viewer, or mutation request; that
count is stated here so the authorization is read as the read it is. No
credential, header, or vault is involved. Claimed straight into
`30-in-progress/` by the integrator; the unattended half — a tests-enabled
live-candidate flag over the production factories and the probe verdict for
a shipped enabled selection — runs as a worker slice, then the integrator
performs the authorized read once, records it, and lands the macOS flip with
the enabled course in the same commit, as the required final state orders.
