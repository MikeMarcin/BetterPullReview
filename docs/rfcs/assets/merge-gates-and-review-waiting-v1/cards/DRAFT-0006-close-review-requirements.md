---
schema: "implement-rfc.card.v1"
id: "DRAFT-0006"
title: "Verify and close the reviewer and merge-requirements RFC"
status: "Backlog"
rfcs: ["RFC-DRAFT-0008"]
depends_on: ["DRAFT-0004", "DRAFT-0005"]
closeout_for: ["RFC-DRAFT-0008"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"]}
---

# Verify and close the reviewer and merge-requirements RFC

## Scope and acceptance

Reconcile all linked criteria and the applicable provider/Overview amendment. Name which host and transport paths were qualified; current GitHub.com documentation is not live GHES or private-organization evidence.

Authoritative acceptance: [RFC-DRAFT-0008](../RFC-DRAFT-0008-reviewers-and-merge-requirements.md#acceptance), AC01, AC02, AC03, AC04, AC05, AC06. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Retain the named-team criterion if a supported path fails it; do not turn a generic warning into acceptance. Reuse unchanged passing checks, run documentation verification and preserve unrelated native/human obligations.

Follow the [canonical milestone plan](../milestone-plan.json), [entrypoint](../IMPLEMENTATION_PROMPT.md) and [proposed amendments](../PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
