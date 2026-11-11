---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "Address feedback and the pull request association"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: ["BPR-2021", "BPR-2030"]
metadata: {"criteria": ["AC01", "AC02", "AC03"], "milestone_plan": {"id": "address-review-feedback-v1", "path": "docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"}}
---

# Address feedback and the pull request association

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0001](../RFC-DRAFT-0001-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md), criteria `AC01`, `AC02`, `AC03`.

The control on a Returned row and on an authored pull request's header; opening the associated local draft on Feedback; creating an associated local draft from a chosen worktree after verifying repository and branch; and the accepted, ahead, behind and diverged head states. Nothing in the worktree changes and no agent starts. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for when the control is offered and for each head relation. The composed path on real temporary repositories: a matching worktree, one ahead, one behind, one diverged, a wrong repository and a wrong branch. The worktree is compared before and after.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
