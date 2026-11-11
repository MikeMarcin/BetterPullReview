---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "Evaluate feedback"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: ["DRAFT-0002", "BPR-2026"]
metadata: {"criteria": ["AC01", "AC02", "AC07", "AC08"], "milestone_plan": {"id": "address-review-feedback-v1", "path": "docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"}}
---

# Evaluate feedback

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0002](../RFC-DRAFT-0002-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md), criteria `AC01`, `AC02`, `AC07`, `AC08`.

The read-only run over open or selected items; the proposed disposition, reason and plan per item, marked as the agent's; accept per item and for all; the not-evaluated state; reviewers' text carried as marked quoted data in the brief; and the built-in prompt `local-draft.feedback-evaluate`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

A fixture agent returning each disposition, a partial answer and no answer. A conversation that contains an instruction to the agent: the launch arguments, grant and run kind are compared with a run without it and are equal. Prompt tests in the shape of the existing writing-hook ones.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
