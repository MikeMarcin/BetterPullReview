---
schema: "implement-rfc.card.v1"
id: "DRAFT-0006"
title: "Suggest the change from a draft comment"
status: "Backlog"
rfcs: ["RFC-DRAFT-0003"]
depends_on: []
metadata: {"criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# Suggest the change from a draft comment

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0003](../RFC-DRAFT-0003-turn-a-draft-comment-into-an-exact-suggested-change.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The control on an eligible draft comment; the seventh comment-workshop operation; the structured reply and core's acceptance checks; the hand-over into RFC-BPR-0090's in-pane suggestion edit; the refusal and its reason; Undo; and its own built-in prompt, `review.suggest-change`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests over every acceptance check: a differing range, one differing byte in the stated current text, an identical replacement, an out-of-bounds one and a change GitHub cannot take. A fixture agent producing an applying reply, a refused one and a reply that arrives after the head moved. The owner's look (AC06) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
