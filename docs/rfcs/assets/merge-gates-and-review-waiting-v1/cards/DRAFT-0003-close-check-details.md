---
schema: "implement-rfc.card.v1"
id: "DRAFT-0003"
title: "Verify and close the check-details RFC"
status: "Backlog"
rfcs: ["RFC-DRAFT-0007"]
depends_on: ["DRAFT-0001", "DRAFT-0002"]
closeout_for: ["RFC-DRAFT-0007"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"]}
---

# Verify and close the check-details RFC

## Scope and acceptance

Reconcile every criterion of the linked RFC against direct evidence for the integrated feature. Reconcile the relevant provider and Overview documentation amendments.

Authoritative acceptance: [RFC-DRAFT-0007](../RFC-DRAFT-0007-inspect-commit-checks.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Reuse unchanged focused evidence and run the documentation gate. Leave failed, unsupported or genuinely human-only obligations explicit. This draft contains no implementation baseline or passing evidence.

Follow the [canonical milestone plan](../milestone-plan.json), [entrypoint](../IMPLEMENTATION_PROMPT.md) and [proposed amendments](../PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
