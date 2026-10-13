---
schema: "implement-rfc.card.v1"
id: "BPR-1455"
title: "Build the differential harness that checks the Git metadata readers against Git"
status: "Backlog"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1455: Build the differential harness that checks the Git metadata readers against Git

The harness of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) (Evaluation method, Oracle and
Corpus). Tests may run Git; the product may not. The baseline is the fixed
readers from [[BPR-1454]].

## Acceptance criteria

- **BPR-1455:AC1:** A workspace contract executable builds each corpus repository with the installed Git and asks Git (with `GIT_CONFIG_NOSYSTEM=1`, an empty global config, empty `HOME` and `XDG_CONFIG_HOME`, and `GIT_OPTIONAL_LOCKS=0`) and the first-party readers the C1–C6 questions, comparing the answers per path or reference and reporting agreement per component and case.
- **BPR-1455:AC2:** The corpus covers the RFC's object formats, index variants, index entries, repository layouts, HEAD states, config, ignore and attribute cases and the adversarial patterns. A case whose feature the installed Git lacks reports skipped with the version found, never a pass.
- **BPR-1455:AC3:** Every divergence is classified as a fix, an explicit refusal with a reason, or a documented deliberate choice; an unclassified divergence fails the row (H1 for K).
- **BPR-1455:AC4:** A negative control, a scratch build with one [[BPR-1454]] fix reverted, turns the harness red; the result is recorded.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
