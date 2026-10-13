---
schema: "implement-rfc.card.v1"
id: "BPR-1457"
title: "Fuzz oracles, adversarial timing and the 250,000-entry bench for the Git metadata readers"
status: "Backlog"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1457: Fuzz oracles, adversarial timing and the 250,000-entry bench for the Git metadata readers

The Fuzzing and Performance paragraphs of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) for K
on macOS.

## Acceptance criteria

- **BPR-1457:AC1:** Every reader that stays keeps a target in the existing fuzz harness; the index target asserts a serialize-then-parse round trip, and the config and matcher targets assert properties beyond the absence of a crash.
- **BPR-1457:AC2:** The BPR-0970 input and the adversarial corpus each finish within 1 s on the fixed readers (H6 baseline).
- **BPR-1457:AC3:** The scale bench builds a 250,000-entry checkout and reports the median of five runs of index parsing and an ignore walk, plus peak memory, recorded for the decision record without asserting it in ctest.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
