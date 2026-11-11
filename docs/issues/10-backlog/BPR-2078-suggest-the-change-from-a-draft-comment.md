---
schema: "implement-rfc.card.v1"
id: "BPR-2078"
title: "Suggest the change from a draft comment"
status: "Backlog"
rfcs: ["RFC-BPR-0118"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0006-suggest-the-change-from-a-draft-comment.md"}
depends_on: []
---

# Suggest the change from a draft comment

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0118](../../rfcs/RFC-BPR-0118-turn-a-draft-comment-into-an-exact-suggested-change.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The control on an eligible draft comment; the seventh comment-workshop operation; the structured reply and core's acceptance checks; the hand-over into RFC-BPR-0090's in-pane suggestion edit; the refusal and its reason; Undo; and its own built-in prompt, `review.suggest-change`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests over every acceptance check: a differing range, one differing byte in the stated current text, an identical replacement, an out-of-bounds one and a change GitHub cannot take. A fixture agent producing an applying reply, a refused one and a reply that arrives after the head moved. The owner's look (AC06) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).
