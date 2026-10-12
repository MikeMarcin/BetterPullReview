---
schema: "implement-rfc.card.v1"
id: "BPR-1395"
title: "Verify and close RFC-BPR-0069"
status: "Backlog"
rfcs: ["RFC-BPR-0069"]
depends_on: ["BPR-1393", "BPR-1394"]
closeout_for: ["RFC-BPR-0069"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"]}
---

# Verify and close RFC-BPR-0069

## Scope and acceptance

Reconcile all linked criteria and the applicable provider/Overview amendment. Name which host and transport paths were qualified; current GitHub.com documentation is not live GHES or private-organization evidence.

Authoritative acceptance: [RFC-BPR-0069](../../rfcs/RFC-BPR-0069-reviewers-and-merge-requirements.md#acceptance), AC01, AC02, AC03, AC04, AC05, AC06. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Retain the named-team criterion if a supported path fails it; do not turn a generic warning into acceptance. Reuse unchanged passing checks, run documentation verification and preserve unrelated native/human obligations.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.

RFC base: `9857d596769468c0585c475467175e3998a33530`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** A composed PR with requested users and teams that have never acted displays every returned request. A prior approval followed by a re-request retains both facts; removal, dismissal and replacement update the roster by identity without confusing pending reviews with requests. | Pending composed evidence | pending | — |
| **AC02:** With a blocked review requirement and three provider-flagged code-owner team requests, Overview visibly names all three teams and the requirement. It neither replaces them with an aggregate warning nor claims that every listed team must separately approve without such evidence. | Pending composed evidence | pending | — |
| **AC03:** The reported failure/approval scenario exposes its simultaneous blockers, applicable rule origins and check details. A missing required result, an optional failure, classic protection, inherited active rules and inactive rules remain distinguishable as defined above. | Pending composed evidence | pending | — |
| **AC04:** Required-minimum, remaining-approval and auto-merge statements follow the evidence boundary. Historical approvals or request counts cannot produce a false remaining count, satisfied gate or enabled auto-merge claim. | Pending composed evidence | pending | — |
| **AC05:** Permission limits, unsupported host fields, partial connections, new heads/targets and late completions retain truthful field availability and usable PR content across native and supported CLI transports. A known merge block cannot disappear because its detailed explanation is unavailable. | Pending composed evidence | pending | — |
| **AC06:** The rendered requirements and roster are readable and operable through ordinary Overview controls, keyboard and accessibility with long team names, narrow widths and enlarged text. Inspecting them performs only admitted reads and explicitly requested external-link handoffs. | Pending composed evidence | pending | — |
