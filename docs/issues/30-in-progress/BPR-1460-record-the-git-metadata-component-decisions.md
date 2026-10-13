---
schema: "implement-rfc.card.v1"
id: "BPR-1460"
title: "Record each Git metadata component's verdict in a committed decision record"
status: "In Progress"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458", "BPR-1459"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1460: Record each Git metadata component's verdict in a committed decision record

The Decision rules of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) applied per component C1–C6.
Verdicts that need Windows or Linux evidence (H2, H5) or an open owner answer
(Q2, Q3) are recorded as pending on that evidence or answer, never assumed.

## Acceptance criteria

- **BPR-1460:AC1:** A committed decision record gives each component's verdict, the gate or rule that decided it, links to the evidence, and every question answer it relies on; a candidate that passes but needs an open answer is recorded as eligible pending that answer.
- **BPR-1460:AC2:** If anything moves, the system architecture and the workspace adapter's description are updated, any adopted library sits behind the workspace-adapter port with no type crossing inward, and the `core-only` build, the architecture checks and the documentation gate pass.

## Continuation

Done on `f965a7693`: the [decision record](../../verification/RFC-BPR-0083-decision.md)
keeps K for C1–C6 and records what could change a verdict. K's Windows and
Linux H2 and H5 cells are pending [[BPR-1462]] and [[BPR-1463]]; a failure
there is a K defect to fix, since no other candidate is eligible.
