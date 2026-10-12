---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "Normalize inspectable current check observations"
status: "Backlog"
rfcs: ["RFC-DRAFT-0007"]
depends_on: []
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04"]}
---

# Normalize inspectable current check observations

## Scope and acceptance

Own provider/application check observations and their deterministic identity and availability contract. Carry current check-run and commit-status records, descriptions and admitted destination inputs under existing budgets. Preserve producer and evaluated-commit identity; do not infer requiredness from outcome.

Authoritative acceptance: [RFC-DRAFT-0007](../RFC-DRAFT-0007-inspect-commit-checks.md#acceptance), AC01, AC02, AC03, AC04. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Use focused provider/normalizer and stale-acceptance evidence. Keep raw provider and credential types outside core and Qt. The next card composes this through the ordinary Overview.

Follow the [canonical milestone plan](../milestone-plan.json), [entrypoint](../IMPLEMENTATION_PROMPT.md) and [proposed amendments](../PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
