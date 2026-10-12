---
schema: "implement-rfc.card.v1"
id: "BPR-1393"
title: "Normalize pending reviewers and merge requirements"
status: "Done"
rfcs: ["RFC-BPR-0069"]
depends_on: ["BPR-1392"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"219055819eb4207abb94c880f433402526ffe126","evidence":{"kind":"command","detail":"Provider/application scope: four focused suites pass; GitHub.com/GHES3.18 synthetic normalization, named actors/asCodeOwner, separate policy/assessment, pagination/limits, stale/removal/late identity and closed native/CLI read admission. Rendered completion belongs to BPR-1394."}},{"criterion":"AC02","outcome":"pass","revision":"219055819eb4207abb94c880f433402526ffe126","evidence":{"kind":"command","detail":"Provider/application scope: four focused suites pass; GitHub.com/GHES3.18 synthetic normalization, named actors/asCodeOwner, separate policy/assessment, pagination/limits, stale/removal/late identity and closed native/CLI read admission. Rendered completion belongs to BPR-1394."}},{"criterion":"AC03","outcome":"pass","revision":"219055819eb4207abb94c880f433402526ffe126","evidence":{"kind":"command","detail":"Provider/application scope: four focused suites pass; GitHub.com/GHES3.18 synthetic normalization, named actors/asCodeOwner, separate policy/assessment, pagination/limits, stale/removal/late identity and closed native/CLI read admission. Rendered completion belongs to BPR-1394."}},{"criterion":"AC04","outcome":"pass","revision":"219055819eb4207abb94c880f433402526ffe126","evidence":{"kind":"command","detail":"Provider/application scope: four focused suites pass; GitHub.com/GHES3.18 synthetic normalization, named actors/asCodeOwner, separate policy/assessment, pagination/limits, stale/removal/late identity and closed native/CLI read admission. Rendered completion belongs to BPR-1394."}},{"criterion":"AC05","outcome":"pass","revision":"219055819eb4207abb94c880f433402526ffe126","evidence":{"kind":"command","detail":"Provider/application scope: four focused suites pass; GitHub.com/GHES3.18 synthetic normalization, named actors/asCodeOwner, separate policy/assessment, pagination/limits, stale/removal/late identity and closed native/CLI read admission. Rendered completion belongs to BPR-1394."}}]
---

# Normalize pending reviewers and merge requirements

## Scope and acceptance

Own the bounded provider/application observations for current review requests, code-owner origin, typed user/team identity, aggregate review/merge state and applicable branch requirements. Reconcile the narrow compiled GraphQL read amendment and transport capability admission.

Authoritative acceptance: [RFC-BPR-0069](../../rfcs/RFC-BPR-0069-reviewers-and-merge-requirements.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Exercise documented response shapes, complete pagination and capability failures through the provider facade. Request-origin and policy evidence must remain distinct; accessible code-owner names are required, with actual unavailable facts separately represented.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.

## Execution

Provider/application slice accepted at `219055819eb4207abb94c880f433402526ffe126`. The four focused provider, contract, host and closed CLI suites pass. Named code-owner requests, ordinary users and hidden identities, applicable inherited rules/classic protection, aggregate merge/review/auto-merge observations, full roster pagination and repeated-cursor refusal are exercised. Repeated failed refreshes retain same-head facts as stale; accepted removals and replacement identities update them. Optional observations have a separate 8 MiB/32-page budget and cannot erase essential PR reads or their diagnostics. Overview acceptance continues on BPR-1394; no live private-organization or native GHES parity is claimed.
