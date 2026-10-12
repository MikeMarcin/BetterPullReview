---
schema: "implement-rfc.card.v1"
id: "BPR-1395"
title: "Verify and close RFC-BPR-0069"
status: "Done"
rfcs: ["RFC-BPR-0069"]
depends_on: ["BPR-1393", "BPR-1394"]
closeout_for: ["RFC-BPR-0069"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC01 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}},{"criterion":"AC02","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC02 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}},{"criterion":"AC03","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC03 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}},{"criterion":"AC04","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC04 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}},{"criterion":"AC05","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC05 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}},{"criterion":"AC06","outcome":"pass","revision":"96977984518c41dadbe8810325bcbd2f0268047c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0069.md","detail":"AC06 integrated criterion evidence, exact product revision and qualifications in the ledger; full pack independent critical review remains pending."}}]
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
| **AC01:** A composed PR with requested users and teams that have never acted displays every returned request. A prior approval followed by a re-request retains both facts; removal, dismissal and replacement update the roster by identity without confusing pending reviews with requests. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |
| **AC02:** With a blocked review requirement and three provider-flagged code-owner team requests, Overview visibly names all three teams and the requirement. It neither replaces them with an aggregate warning nor claims that every listed team must separately approve without such evidence. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |
| **AC03:** The reported failure/approval scenario exposes its simultaneous blockers, applicable rule origins and check details. A missing required result, an optional failure, classic protection, inherited active rules and inactive rules remain distinguishable as defined above. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |
| **AC04:** Required-minimum, remaining-approval and auto-merge statements follow the evidence boundary. Historical approvals or request counts cannot produce a false remaining count, satisfied gate or enabled auto-merge claim. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |
| **AC05:** Permission limits, unsupported host fields, partial connections, new heads/targets and late completions retain truthful field availability and usable PR content across native and supported CLI transports. A known merge block cannot disappear because its detailed explanation is unavailable. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |
| **AC06:** The rendered requirements and roster are readable and operable through ordinary Overview controls, keyboard and accessibility with long team names, narrow widths and enlarged text. Inspecting them performs only admitted reads and explicitly requested external-link handoffs. | [Criterion ledger](../../verification/RFC-BPR-0069.md) | pass | `96977984518c41dadbe8810325bcbd2f0268047c` |

## Execution record

`see-review-blockers` is integrated; AC01–AC06 and all three authored exits pass.
[The criterion ledger](../../verification/RFC-BPR-0069.md) binds the exact product,
commands, failure corrections, source/transport qualifications and inspected
native-control pixels. Required whole-pack independent critical review remains
pending for the coordinator after `keep-home-waiting`; it is not self-certified.
No human criterion, attended server or protected candidate remains in this milestone.

The durable [handoff](../_checkpoints/merge-gates-and-review-waiting-v1/milestone-2-handoff.json)
records candidates, changed paths, verification, command ownership and continuation.
Next is RFC-BPR-0070 / BPR-1396–1398; BPR-1396 becomes eligible when this closeout
is Done. Preserve the immutable baseline and unrelated dirty state.

## Post-review follow-up

The final independent report reproduced a criterion counterexample on
`15365e9df15ff51b366f9735c887dccdd80cae9e`.
[BPR-1399](BPR-1399-correct-merge-gates-critical-review.md)
corrects the three supplied pack findings at
`eae174cb95e7b66c97739b0a8dd1e6c5ac71af2b`, with direct failing/passing
regressions in [the review/disposition record](../_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json)
and the RFC ledgers. This historical Done slice and its earlier evidence remain
intact. Coordinator-owned narrow independent confirmation and final pack
acceptance remain pending.

## Final independent review consumption

The coordinator supplied and consumed `/root/critical_review`’s narrow independent
confirmation (`rfc-reviewer-critical`, `gpt-6-astra`, `xhigh`) on
`fea984d3148bbdab37ee8606b0cbc3121d6635a4`. C01/C02/C03 are resolved,
with no remaining blocking or nonblocking findings in the corrected counterexamples.
The reviewer’s independent probes and focused 5/5 pass are retained in
[the review record](../_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json).
[Final pack closeout](../_checkpoints/merge-gates-and-review-waiting-v1/pack-closeout.json)
accepts the authorized pack under `merge-gates-r1`; earlier pending-review notes
and original failure evidence remain historical. Existing qualification limits
and unrelated obligations are unchanged. All reviewer/engineering commands are
quiescent; no broad tests or product changes were made for this final bookkeeping.
