---
schema: "implement-rfc.card.v1"
id: "BPR-1397"
title: "Explain pending reviews and concurrent blockers on Home"
status: "Backlog"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1396"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"]}
---

# Explain pending reviews and concurrent blockers on Home

## Scope and acceptance

Own the native Home row/section explanations and filtered summaries from the corrected core projection. Show pending users or code-owner teams and any concurrent check block, preserving grouping, selection and exact opening destination.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Run the composed Home → Overview → Home scenario, including unread activity and its clearance, a final request disappearing while a requirement remains, and long team names. Check Quiet-items behavior and unchanged notification/enrollment boundaries.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
