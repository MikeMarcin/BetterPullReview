---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "What changed since my review"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: []
metadata: {"criteria": ["AC01", "AC02", "AC05", "AC06"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# What changed since my review

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0001](../RFC-DRAFT-0001-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC01`, `AC02`, `AC05`, `AC06`.

The control beside Seen and Approve again and among the Agent tab's suggestions; the `analyze_latest_upload` send with both revisions named; the unavailable state when the earlier revision cannot be read; and its own built-in prompt, `review.latest-upload`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for when the control is offered and for the two revisions it names. The composed path on a fixture pull request with commits newer than a review, and on one whose earlier commits are gone. The launch inventory shows no agent started by opening or refreshing.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
