---
schema: "implement-rfc.card.v1"
id: "BPR-2074"
title: "Was this addressed? on a review thread"
status: "Done"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC03","AC04","AC05","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0002-was-this-addressed-on-a-thread.md","superseded_by":["RFC-BPR-0128","BPR-2165"],"owner_closure":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","decision":"close as superseded"}}
depends_on: ["BPR-2073"]
acceptance: [{"criterion":"AC03","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"core_returning_review, core_agent_chat_scope; application_returning_review_addressed_public_path: only the line-12 thread offers it; the press, with no selection, asks evaluate_proposed_fix about that conversation with its lines then and now (then=1 now=1)"}},{"criterion":"AC04","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"core_returning_review (four words, start only); addressed course shows \"Agent's claim: Partly\"; review mutation evidence unchanged (nothing resolved or posted)"}},{"criterion":"AC05","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"application_agent_launch_policy (review.proposed-fix own prompt, version, isolation, Reset); stub echoes system=2"}},{"criterion":"AC07","outcome":"fail","revision":"72288736c","evidence":{"kind":"artifact","detail":"Rejected by the owner (decision e789321a). Attended session ff82755e1b9fba2463ef8b257a6de0f5 (state /Users/jaeger/.attended-review-state/returning-review-v001-2026-11-12/state), candidates BPR-2073-what-changed and BPR-2074-was-addressed on frozen root /Users/jaeger/returning-review-v001.34b39a8g"}}]
---

# Was this addressed? on a review thread

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC03`, `AC04`, `AC05`, `AC07`.

The control on a thread and among its Ask agent prompts; `evaluate_proposed_fix` taking a conversation as its subject, with core's selection rule and `docs/ux/ai-experience.md` changed to say so; the brief's lines then and now; the four-word lead shown as a claim; and its own built-in prompt, `review.proposed-fix`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for the changed subject rule. A fixture agent answering each of the four words; the thread's resolution and any defect disposition are compared before and after and are equal. The owner's look (AC07) is an attended review on a frozen build that opens on the returned pull request.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Owner look

AC07 rejected in attended session `ff82755e…` (decision `e789321a`, 2026-11-13), verbatim:

> I dunno it doesn't read well with a narrow drawer thats for sure. I don't know why 12 would get it but 6 doesnt. 
> Just doesn't seem like a great place for this.
>
> I think we can do better.

Recorded only; another round reworks it.

## Hygiene audit 2026-11-15

Verdict: **SUPERSEDED**. By RFC-BPR-0128 (Auto-Verified) through BPR-2165 (Done): Was this addressed? on any thread, a file or the pull request, with a claim chip. That answers the owner's AC07 Reject ('I think we can do better').
This is a read-only tracker audit: it changes no status, folder or metadata.

## Owner decision 2026-11-16: closed as superseded

Owner, 2026-11-16 (on the hygiene audit, relayed by the coordinator): close
this card as superseded. The board has one terminal lane, so a superseded card
sits in Done with `metadata.superseded_by` (as [[BPR-1353]] does); it was not
delivered as written.

By [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md)
(Auto-Verified 2026-11-14) through [[BPR-2165]] (Done): Was this addressed?
on any review thread, at a file and at the pull request, with the agent's
claim as a chip in the thread's header (owner Accept `BPR-2165-claim-chips`,
ask-v002; the many-thread answer accepted at ask-v008). That is the owner's
answer to this card's AC07 Reject, "I think we can do better". The AC03 to
AC05 records above stay as they were; AC07 stays failed against its own
revision and was never re-asked on this card. RFC-BPR-0116 closes through
RFC-BPR-0128 on [[BPR-2075]].
