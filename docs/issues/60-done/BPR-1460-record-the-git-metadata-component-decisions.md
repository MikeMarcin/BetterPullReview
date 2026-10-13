---
schema: "implement-rfc.card.v1"
id: "BPR-1460"
title: "Record each Git metadata component's verdict in a committed decision record"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458", "BPR-1459"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1460:AC1","outcome":"pass","revision":"f965a7693","evidence":{"kind":"observation","detail":"docs/verification/RFC-BPR-0083-decision.md (commit f965a7693) gives each component C1-C6 the verdict Keep K, the gate or rule that decided it (rule 1; L fails H1/H2/H3/H6, V fails H6 and needs Q1 approval, G rejected; C3's reftable capability has no candidate), links to the harness, reader rows, canary course, H6 table, scale bench and L/V/G cost records in the ledger, and every question answer relied on (Q1 answered, Q2 a, Q3 not asked because L passes no component). Pending: K's H2/H5 cells on Windows and Linux (BPR-1462, BPR-1463).","path":"docs/issues/60-done/BPR-1460-record-the-git-metadata-component-decisions.md"}},{"criterion":"BPR-1460:AC2","outcome":"pass","revision":"f965a7693","evidence":{"kind":"observation","detail":"Nothing moved, so the system architecture and workspace adapter description need no change beyond BPR-1454's domain-model note; no library is adopted, libgit2 stays behind the evaluation-only option. On the current head: core-only preset built with every code row passing (the failures are three documentation rows needing .agents scripts absent from worktrees, and two source scans flagging other agents' files on main: tests/hash_evaluation/run_hash_evaluation.py and tests/adapters_qt/conversation_cards_view_tests.cpp); architecture_boundaries passes in the dev tree; DOCUMENTATION_OK.","path":"docs/issues/60-done/BPR-1460-record-the-git-metadata-component-decisions.md"}}]
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
