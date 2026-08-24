---
id: BPR-0314
title: Verify anonymous preview remains unavailable on Linux
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0306]
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

## Partial target-native evidence — 2026-08-22

Three of this card's rows are now produced on target-native Linux. They are
recorded as advance evidence; the card stays open, and none of this is an
enablement claim.

None of it was obtainable before today. Every row needs the tests-disabled
Release binary, which did not build on Linux at all until [[BPR-0421]] and
[[BPR-0440]] landed — [[BPR-0156]] recorded that preset as "does not compile".

Host: Ubuntu 26.04 x86_64, GCC 15.2.0, `main` after the Windows-lane merge.
Binary: `build/app-release/src/app/better_pull_review`, 10,535,800 bytes.

| Row | Command | Result |
|---|---|---|
| clean Linux target builds the tests-disabled Release application | `cmake --build --preset app-release -- -k 0` | exit 0, links |
| fixture-seam absence | `scripts/verify_rfc0017_test_inventory.py --build-root build/dev --release-binary …` | `RFC0017_TEST_INVENTORY_OK` |
| dependency / ELF | `scripts/verify_binary_dependencies.py …` | `BINARY_DEPENDENCIES_OK` |
| runtime | `scripts/verify_runtime_surface.py …` | `RUNTIME_SURFACE_OK` |

The fixture-seam row is the one worth noting: that script exists precisely to
prove the RFC-BPR-0017 evidence selectors are registered once in a tests-enabled
build and **absent** from a tests-disabled Release binary. It has presumably
never run against a Linux Release binary before, because there was none.

### Still outstanding, and why this card cannot close

- Typed public-preview unavailability selected for GitHub.com and GHES, and the
  exact zero-effect counters for vault/keyring, Authorization header,
  REST/GraphQL, Git smart HTTP, browser, mutation, child-process and
  private-content. Both need the RFC-BPR-0017 code that does not exist yet.
- Qt presentation and the hygiene matrix.
- [[BPR-0306]], this card's declared dependency, is in `10-backlog/` behind a
  chain rooting at [[BPR-0298]] (`awaiting-live-github`) and at [[BPR-0035]] →
  [[BPR-0036]] (target-native Windows).
- `docs/rfcs/future/RFC-BPR-0017-unauthenticated-public-review-read.md` gates
  implementation on "RFC-BPR-0002 Auto-Verified" and carries no early-
  implementation carve-out, unlike RFC-BPR-0016. Starting the missing work here
  would breach that gate.
