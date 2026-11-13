---
schema: "implement-rfc.card.v1"
id: "BPR-2074"
title: "Was this addressed? on a review thread"
status: "Parked"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC03","AC04","AC05","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0002-was-this-addressed-on-a-thread.md"}
depends_on: ["BPR-2073"]
parked_on: "human-review"
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
