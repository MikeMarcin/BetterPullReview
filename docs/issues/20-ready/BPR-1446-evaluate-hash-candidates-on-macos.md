---
schema: "implement-rfc.card.v1"
id: "BPR-1446"
title: "Evaluate sha1dc, HACL* SHA-2 and platform crypto on macOS arm64"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1446: Evaluate sha1dc, HACL* SHA-2 and platform crypto on macOS arm64

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`hash-evaluation-macos`.

## Outcome

An opt-in, non-shipping evaluation target measures the RFC's candidates
against the current core SHA-1 and SHA-256 on this Mac, and a committed report
applies the decision rules. Nothing enters core, `third_party/`,
`packaging/dependencies.json` or a package. The shipping configuration acquires
nothing new.

## Acceptance

- **AC1 (RFC AC01, macOS):** A committed report records raw results, the CPU,
  its SHA-extension flags, the OS, the compiler and the build, applies the
  decision rules, and gives one adopt, reject or pending decision per
  component. Q1, Q2 and Q3 are recorded as unanswered; a passing candidate is
  "eligible pending Qn".
- **AC2 (RFC AC02 gate before adoption):** Zero mismatches for every candidate
  across the NIST CAVP byte-oriented SHA-1 and SHA-256 vectors, boundary
  lengths, split points and uneven chunking, seeded differential inputs up to
  256 MiB against the baseline and CommonCrypto, `git hash-object` in both
  object formats, trees and commits against `mktree`/`write-tree`/
  `commit-tree`, and every object of this repository's history.
- **AC3 (RFC AC03 evidence before adoption):** sha1dc detects the SHAttered
  pair, the SHA-mbles pair and its own test vectors while a plain-SHA-1 control
  shows each pair's digests equal; the repository sweep detects nothing. What
  detection does to a collision input hashed as a Git object is recorded.
- **AC4 (RFC AC04, macOS):** W1 to W3 throughput under the RFC's conditions
  (Release `-O3`, idle, `-j1`, medians of 7) for each candidate and the
  baseline, plus the macOS window-thread baseline rows a later adoption is
  compared against.
- **AC5:** The candidates are acquired only through pinned, hash-verified
  archives behind an evaluation-only option, `dev` and `core-only` configure
  without it, and the architecture and documentation checks pass.
