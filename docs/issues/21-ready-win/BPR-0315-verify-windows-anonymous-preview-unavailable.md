---
id: BPR-0315
title: Verify anonymous preview remains unavailable on Windows x64
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0036, BPR-0311, BPR-0306]
---

# Verify anonymous preview remains unavailable on Windows x64

## Required final state

- A real supported Windows 10/11 x64 host builds the tests-disabled Release
  application and selects typed public-preview unavailability for GitHub.com
  and GHES.
- Exact counters prove zero Credential Manager, Authorization header,
  REST/GraphQL, Git smart HTTP, browser, mutation, child-process, and private-
  content effects.
- PE/runtime/dependency, Qt presentation, fixture-seam absence, documentation,
  and hygiene gates pass target-native. No enablement claim is made.
- This card consumes and validates RFC-BPR-0016 BPR-0311's existing native
  `windows-app-release` tests-disabled Release preset; it does not create a
  competing preset or alter that preset's test-seam policy.
- This card's first target-native course is provenance for the unavailable
  selection; BPR-0309 reruns the same idempotent course at immutable final head
  after cross-platform simplification and documentation reconciliation.

## Deferring — 2026-08-23

The Windows session that cleared the rest of this lane checked this card's
dependencies: they sit in `10-backlog/`, so nothing here can start. The
card stays Ready, deferring, and is not part of the lane's completed set.
