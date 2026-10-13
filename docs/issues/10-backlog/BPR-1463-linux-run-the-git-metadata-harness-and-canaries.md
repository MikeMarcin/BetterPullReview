---
schema: "implement-rfc.card.v1"
id: "BPR-1463"
title: "Linux: run the Git metadata differential harness, canary course and cost measurements"
status: "Backlog"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458"]
priority: "P2"
platforms: ["linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1463: Linux: run the Git metadata differential harness, canary course and cost measurements

The Linux half of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md)'s H2 and H5 and of AC03 and
AC05, on a native Linux station. Nothing here is measured on macOS.

## Acceptance criteria

- **BPR-1463:AC1:** On native Linux, [[BPR-1454]]'s contract tests and the differential harness pass for K, with every divergence classified, and the canary course records containment, no-execution and no-write results for K and every L component evaluated.
- **BPR-1463:AC2:** With the evaluation option on, libgit2 builds with the existing Linux toolchain, and the stripped binary-size and build-time deltas are measured and recorded in the decision record.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
