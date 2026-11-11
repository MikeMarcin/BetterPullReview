---
schema: "implement-rfc.card.v1"
id: "DRAFT-0010"
title: "Close agent changes in a local draft's worktree"
status: "Backlog"
rfcs: ["RFC-DRAFT-0003"]
depends_on: ["DRAFT-0007", "DRAFT-0008", "DRAFT-0009"]
closeout_for: ["RFC-DRAFT-0003"]
metadata: {"criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06", "AC07", "AC08", "AC09", "AC10", "AC11", "AC12", "AC13"], "milestone_plan": {"id": "local-draft-authoring-v1", "path": "docs/rfcs/assets/local-draft-authoring-v1/milestone-plan.json"}}
---

# Close agent changes in a local draft's worktree

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0003](../RFC-DRAFT-0003-agent-changes-in-a-local-pr-draft-worktree.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`, `AC10`, `AC11`, `AC12`, `AC13`.

Acceptance assessment for agent fixes and history runs on a local draft.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent, and say for each installed agent whether its own runtime enforces the grant or only follows it.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
