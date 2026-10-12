---
schema: "implement-rfc.card.v1"
id: "BPR-1396"
title: "Preserve and classify authored Home waiting facts"
status: "Ready"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1395"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Preserve and classify authored Home waiting facts

## Scope and acceptance

Own core and application composition of per-fact waiting availability, accepted authorship, merge/review summaries, bounded metadata refresh and private persistence. Correct default-zero replacement across open/discovery/retained paths, including Follow-only authored PRs.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Verify accepted-fact ordering, real discovery-to-open regression, unknown versus empty, authority changes and storage upgrade/restart. Keep the existing discovery bounds and RFC0066 precedence; do not load full PR contents for Home or expand team enrollment.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
