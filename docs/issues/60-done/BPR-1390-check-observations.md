---
schema: "implement-rfc.card.v1"
id: "BPR-1390"
title: "Normalize inspectable current check observations"
status: "Done"
rfcs: ["RFC-BPR-0068"]
depends_on: []
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"34f62d7e3ba4f2ddc4e1739c069bea7f45b5084c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0068.md","detail":"Observation-slice evidence: focused provider/host contracts, exact current identity/source/details and optional-read refusal; full presentation acceptance remains with BPR-1391."}},{"criterion":"AC02","outcome":"pass","revision":"34f62d7e3ba4f2ddc4e1739c069bea7f45b5084c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0068.md","detail":"Observation-slice evidence: focused provider/host contracts, exact current identity/source/details and optional-read refusal; full presentation acceptance remains with BPR-1391."}},{"criterion":"AC03","outcome":"pass","revision":"34f62d7e3ba4f2ddc4e1739c069bea7f45b5084c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0068.md","detail":"Observation-slice evidence: focused provider/host contracts, exact current identity/source/details and optional-read refusal; full presentation acceptance remains with BPR-1391."}},{"criterion":"AC04","outcome":"pass","revision":"34f62d7e3ba4f2ddc4e1739c069bea7f45b5084c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0068.md","detail":"Observation-slice evidence: focused provider/host contracts, exact current identity/source/details and optional-read refusal; full presentation acceptance remains with BPR-1391."}}]
---

# Normalize inspectable current check observations

## Scope and acceptance

Own provider/application check observations and their deterministic identity and availability contract. Carry current check-run and commit-status records, descriptions and admitted destination inputs under existing budgets. Preserve producer and evaluated-commit identity; do not infer requiredness from outcome.

Authoritative acceptance: [RFC-BPR-0068](../../rfcs/RFC-BPR-0068-inspect-commit-checks.md#acceptance), AC01, AC02, AC03, AC04. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Use focused provider/normalizer and stale-acceptance evidence. Keep raw provider and credential types outside core and Qt. The next card composes this through the ordinary Overview.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.
