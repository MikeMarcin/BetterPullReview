---
schema: "implement-rfc.card.v1"
id: "BPR-1456"
title: "Extend the canary course with containment canaries for the Git metadata readers on macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1456: Extend the canary course with containment canaries for the Git metadata readers on macOS

The Safety paragraph of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) for K on macOS.
Windows and Linux runs are [[BPR-1462]] and [[BPR-1463]].

## Acceptance criteria

- **BPR-1456:AC1:** Canaries outside the allowed scope detect any open, including a FIFO named by `include.path` and a symlinked `.gitignore` and `.gitattributes` pointing outside the root, alongside the existing hook and `core.fsmonitor` canaries.
- **BPR-1456:AC2:** After every harness case the checkout is byte-identical, unmodified and free of lock files, and no canary fired, for K on macOS.

## Continuation

Done for K on macOS on `9a8cde4f5`; evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1455-and-bpr-1456-the-differential-harness-and-canary-course).
L's results from the same course are [[BPR-1458]]'s. Windows and Linux runs are
[[BPR-1462]] and [[BPR-1463]].
