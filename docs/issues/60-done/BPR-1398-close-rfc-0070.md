---
schema: "implement-rfc.card.v1"
id: "BPR-1398"
title: "Verify and close RFC-BPR-0070"
status: "Done"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1396", "BPR-1397"]
closeout_for: ["RFC-BPR-0070"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"551bce4e0d66bcc2b5743eba63a779da9088007e","evidence":{"kind":"observation","detail":"Native and CLI discovery → accepted Overview → Home courses preserve three named code-owner requests; host unread/read transitions preserve fields. Final public6/6, clearance5/5.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log","/tmp/bpr-merge-gates-run/m3-limit-fix-tests.log"]}},{"criterion":"AC02","outcome":"pass","revision":"551bce4e0d66bcc2b5743eba63a779da9088007e","evidence":{"kind":"observation","detail":"Exact provider actor ownership is projection-only for Follow and observer enrollment; aggregate review with no names waits; Unfollow removes a Follow-only row; numeric-account isolation passes.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log","/tmp/bpr-merge-gates-run/m3-limit-fix-tests.log"]}},{"criterion":"AC03","outcome":"pass","revision":"551bce4e0d66bcc2b5743eba63a779da9088007e","evidence":{"kind":"observation","detail":"Three-team Branch Freshness public example remains waiting; host unread/clearance and complete removal/approval preserve independent blockers; native Quiet/accessibility checks pass.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log","/tmp/bpr-merge-gates-run/m3-limit-fix-tests.log"]}},{"criterion":"AC04","outcome":"pass","revision":"551bce4e0d66bcc2b5743eba63a779da9088007e","evidence":{"kind":"observation","detail":"Host/provider/storage dispatch order, omitted versus empty, head/account/session, unavailable refresh, optional write failure, restart/migration and bounded minimal reads pass; final codec recheck2/2. Final optional-summary page/byte exhaustion negative controls preserve usable mandatory facts and report partial detail.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log","/tmp/bpr-merge-gates-run/m3-limit-fix-tests.log"]}},{"criterion":"AC05","outcome":"pass","revision":"551bce4e0d66bcc2b5743eba63a779da9088007e","evidence":{"kind":"observation","detail":"Native role/Quiet/collapsed counts/accessibility and composed exact selection/open/sibling/filter/Recent courses pass; Home slow-open passes and final Overview scales1/1.25/2 pass. Notification policy unchanged.","refs":["docs/verification/RFC-BPR-0070.md","/tmp/bpr-merge-gates-run/m3-candidate-tests.log","/tmp/bpr-merge-gates-run/m3-codec-tests.log","/tmp/bpr-merge-gates-run/m3-limit-fix-tests.log"]}}]
---

# Verify and close RFC-BPR-0070

## Scope and acceptance

Reconcile every criterion of the Home RFC and its explicit RFC0066 dependency. Finish the proposed Home/queue prose amendments and reconcile final pack coverage without rewriting historical evidence.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; the closeout checklist and accepted records below bind the verified implementation.

## Verification and handoff

Use one integration-focused pass across the three final outcomes plus the documentation gate, reusing unchanged milestone evidence. Preserve true platform and external limits. Live IDs, a real baseline, source revisions and acceptance records are created only during authorized execution.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md).

RFC base: `9857d596769468c0585c475467175e3998a33530`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** An unopened authored PR discovered with pending reviews appears in Waiting on others. Opening, refreshing and marking its discussion read cannot replace its known requests with zero; Home and Overview agree. | [Criterion ledger](../../verification/RFC-BPR-0070.md) and final public/host/storage evidence | pass | `551bce4e0d66bcc2b5743eba63a779da9088007e` |
| **AC02:** An authored PR enrolled only by Follow or an observer rule gets the same waiting placement without saving its repository. An aggregate unmet-review requirement with no named request still produces a truthful waiting row; an unrelated account or team membership cannot supply that role. | [Criterion ledger](../../verification/RFC-BPR-0070.md) and final public/host/storage evidence | pass | `551bce4e0d66bcc2b5743eba63a779da9088007e` |
| **AC03:** The mixed Branch Freshness/code-owner example remains in Waiting on others with its team names and failure visible. Unread activity and its clearance follow the defined My pull requests precedence. Removing the last request does not hide an independently remaining review requirement or check problem behind Quiet. | [Criterion ledger](../../verification/RFC-BPR-0070.md) and final public/host/storage evidence | pass | `551bce4e0d66bcc2b5743eba63a779da9088007e` |
| **AC04:** Interleaved discovery/open/retained reads, head/account changes, incomplete or failed refresh, restart and old stored rows follow the availability rules. A complete newer removal or approval clears only the facts it actually settles; Home refresh stays bounded without full PR loads. | [Criterion ledger](../../verification/RFC-BPR-0070.md) and final public/host/storage evidence | pass | `551bce4e0d66bcc2b5743eba63a779da9088007e` |
| **AC05:** Composed Home preserves exact row selection, opening destination, accurate filtered/grouped counts, collapsed summaries and accessible waiting text. The Quiet-items filter, existing notification policy, unrelated enrollment and Recently viewed behavior follow the boundaries above. | [Criterion ledger](../../verification/RFC-BPR-0070.md) and final public/host/storage evidence | pass | `551bce4e0d66bcc2b5743eba63a779da9088007e` |

## Final pack obligation

The implementation mandate `merge-gates-r1` explicitly requires an independent critical review at the end of the complete pack. The coordinator dispatches that review separately; final pack acceptance must preserve and consume its actual findings and any verified corrections. The first milestone’s self-review and Auto-Verified status do not satisfy this obligation.

## Automated milestone outcome

All five RFC criteria and three milestone exits pass. Facts candidate
`393dfe602354b189a6e883c92dc66bdc6a66579f`, native candidate
`b5b08b3660835a326f16aa62bb25d98c79d89f5c` and final bounded-codec candidate
`39ecdbe1b544e18c12275c95db46734ee3937ecc`, followed by explicit optional-summary
partial status in `551bce4e0d66bcc2b5743eba63a779da9088007e`, are integrated on main. The documentation
gate and focused acceptance/public paths pass. The explicit final pack obligation
above remains pending and belongs to the coordinator's separate critical review;
Done for this component does not mark the whole pack complete.

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
