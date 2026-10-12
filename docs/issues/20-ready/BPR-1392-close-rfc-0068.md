---
schema: "implement-rfc.card.v1"
id: "BPR-1392"
title: "Verify and close RFC-BPR-0068"
status: "Ready"
rfcs: ["RFC-BPR-0068"]
depends_on: ["BPR-1390", "BPR-1391"]
closeout_for: ["RFC-BPR-0068"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0068

## Scope and acceptance

Reconcile every criterion of the linked RFC against direct evidence for the integrated feature. Reconcile the relevant provider and Overview documentation amendments.

Authoritative acceptance: [RFC-BPR-0068](../../rfcs/RFC-BPR-0068-inspect-commit-checks.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Reuse unchanged focused evidence and run the documentation gate. Leave failed, unsupported or genuinely human-only obligations explicit. This draft contains no implementation baseline or passing evidence.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.

RFC base: `9857d596769468c0585c475467175e3998a33530`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** A composed PR containing the reported 24-result example still shows the matching totals and exposes Branch Freshness by name, its failed outcome, and its supplied explanation from ordinary Overview controls. | Pending composed evidence | pending | — |
| **AC02:** Check-run and commit-status examples retain distinct producer, identity and outcome semantics. Repeat attempts, identical names, skipped results and a head/test-merge disagreement produce the current-set behavior defined above, without claiming merge readiness from totals. | Pending composed evidence | pending | — |
| **AC03:** An available result destination opens through the existing admitted host action. Missing/refused destinations and absent output remain readable; unsafe content cannot create an effect or embedded remote load. | Pending composed evidence | pending | — |
| **AC04:** Late responses, head/account changes, pagination limits and failed detail reads cannot present old or incomplete results as current and complete, and do not prevent reading otherwise available PR content. | Pending composed evidence | pending | — |
| **AC05:** The rendered disclosure, result details and actions work by keyboard and accessible names at narrow widths and enlarged text in existing appearances. Accepted updates preserve usable Overview navigation and the user's selection/disclosure state. | Pending composed evidence | pending | — |
