---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "Explain this file, answered in Review"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: []
metadata: {"criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# Explain this file, answered in Review

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0002](../RFC-DRAFT-0002-explain-this-file-where-i-am-reading.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The control on the file header and its context menu; `analyze_current_file` taking the open file as its subject, with core's rule and `docs/ux/ai-experience.md` changed to say so; the answer card at the file header with Keep as note, Continue in Agent tab and Dismiss; and its own built-in prompt, `review.explain-file`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for the changed subject rule. Qt tests at the supported scales for the card and its three actions; the composed path shows the turn in the Agent tab's history. The owner's look (AC06) is an attended review on a frozen build with a fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
