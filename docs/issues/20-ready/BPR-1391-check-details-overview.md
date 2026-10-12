---
schema: "implement-rfc.card.v1"
id: "BPR-1391"
title: "Compose native check details in Overview"
status: "Ready"
rfcs: ["RFC-BPR-0068"]
depends_on: ["BPR-1390"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Compose native check details in Overview

## Scope and acceptance

Own the host projection, semantic detail/link actions and native Overview disclosure. Apply the authoritative presentation and summary semantics to the first card’s observations without changing review navigation or the files table.

Authoritative acceptance: [RFC-BPR-0068](../../rfcs/RFC-BPR-0068-inspect-commit-checks.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Exercise the 24-result example through the real provider/host/window path, then inspect offscreen renders and keyboard/accessibility behavior. Missing output, refused destinations and incomplete results must remain readable.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
