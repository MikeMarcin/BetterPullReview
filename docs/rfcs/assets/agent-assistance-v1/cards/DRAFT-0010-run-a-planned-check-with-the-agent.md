---
schema: "implement-rfc.card.v1"
id: "DRAFT-0010"
title: "Run a planned check with the agent"
status: "Backlog"
rfcs: ["RFC-DRAFT-0005"]
depends_on: ["BPR-2024", "BPR-2068"]
metadata: {"criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06", "AC07"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# Run a planned check with the agent

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0005](../RFC-DRAFT-0005-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

The control on a planned check; the change run that performs it; the reported result with its origin, commands and limitations; the binding rule; acceptance into the evidence record; and its own built-in prompt, `local-draft.run-check`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

A fixture agent that passes, fails, is interrupted and changes a file, against real temporary worktrees; the recorded class is reported in every case. The owner's look (AC07) is an attended review on a frozen build with the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
