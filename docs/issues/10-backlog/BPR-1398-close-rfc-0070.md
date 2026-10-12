---
schema: "implement-rfc.card.v1"
id: "BPR-1398"
title: "Verify and close RFC-BPR-0070"
status: "Backlog"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1396", "BPR-1397"]
closeout_for: ["RFC-BPR-0070"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"]}
---

# Verify and close RFC-BPR-0070

## Scope and acceptance

Reconcile every criterion of the Home RFC and its explicit RFC0066 dependency. Finish the proposed Home/queue prose amendments and reconcile final pack coverage without rewriting historical evidence.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Use one integration-focused pass across the three final outcomes plus the documentation gate, reusing unchanged milestone evidence. Preserve true platform and external limits. Live IDs, a real baseline, source revisions and acceptance records are created only during authorized execution.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
