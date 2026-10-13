---
schema: "implement-rfc.card.v1"
id: "BPR-1458"
title: "Evaluate libgit2 for the Git metadata components behind an evaluation-only option on macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1458: Evaluate libgit2 for the Git metadata components behind an evaluation-only option on macOS

Candidate L of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md). libgit2 is acquired only through
the verified-archive pattern (SHA-256, size, licence digest, re-extraction under
`build/_bpr`) behind an option that is off by default, so the shipping
configuration acquires nothing new. It is built without HTTPS, SSH, NTLM or its
CLI. No libgit2 type crosses its port.

Its provider and adapter need only [[BPR-1454]]; AC2 runs through the harness of [[BPR-1455]] and the canary course of [[BPR-1456]] once they land.

## Acceptance criteria

- **BPR-1458:AC1:** With the option off, no target acquires or links libgit2; with it on, the pinned archive is verified and an evaluation adapter answers the C1–C6 questions for the harness.
- **BPR-1458:AC2:** The harness classifies every L divergence under H1 per component, and the canary course records containment, no-execution and no-write results for every L component evaluated, on macOS.
- **BPR-1458:AC3:** L's cost record lists its license and SPDX expression, a trial `dependencies.json` entry run through `verify_release_sources.py`, the measured macOS stripped binary-size and build-time deltas, its SHA-256 and reftable status, toolchain needs, release and advisory cadence, and integration size.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
