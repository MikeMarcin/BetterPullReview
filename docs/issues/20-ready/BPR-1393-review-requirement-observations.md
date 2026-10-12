---
schema: "implement-rfc.card.v1"
id: "BPR-1393"
title: "Normalize pending reviewers and merge requirements"
status: "Ready"
rfcs: ["RFC-BPR-0069"]
depends_on: ["BPR-1392"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Normalize pending reviewers and merge requirements

## Scope and acceptance

Own the bounded provider/application observations for current review requests, code-owner origin, typed user/team identity, aggregate review/merge state and applicable branch requirements. Reconcile the narrow compiled GraphQL read amendment and transport capability admission.

Authoritative acceptance: [RFC-BPR-0069](../../rfcs/RFC-BPR-0069-reviewers-and-merge-requirements.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Exercise documented response shapes, complete pagination and capability failures through the provider facade. Request-origin and policy evidence must remain distinct; accessible code-owner names are required, with actual unavailable facts separately represented.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
