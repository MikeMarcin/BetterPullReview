---
schema: "implement-rfc.card.v1"
id: "DRAFT-0002"
title: "Was this addressed? on a review thread"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: ["DRAFT-0001"]
metadata: {"criteria": ["AC03", "AC04", "AC05", "AC07"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# Was this addressed? on a review thread

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0001](../RFC-DRAFT-0001-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC03`, `AC04`, `AC05`, `AC07`.

The control on a thread and among its Ask agent prompts; `evaluate_proposed_fix` taking a conversation as its subject, with core's selection rule and `docs/ux/ai-experience.md` changed to say so; the brief's lines then and now; the four-word lead shown as a claim; and its own built-in prompt, `review.proposed-fix`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for the changed subject rule. A fixture agent answering each of the four words; the thread's resolution and any defect disposition are compared before and after and are equal. The owner's look (AC07) is an attended review on a frozen build that opens on the returned pull request.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
