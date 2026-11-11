---
schema: "implement-rfc.card.v1"
id: "DRAFT-0002"
title: "Feedback items, their placement and dispositions"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: ["DRAFT-0001"]
metadata: {"criteria": ["AC04", "AC05", "AC06", "AC07"], "milestone_plan": {"id": "address-review-feedback-v1", "path": "docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"}}
---

# Feedback items, their placement and dispositions

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0001](../RFC-DRAFT-0001-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md), criteria `AC04`, `AC05`, `AC06`, `AC07`.

The Feedback working area; items from unresolved conversations and from changes-requested or commenting reviews with summary text; conversations awaiting the reviewer and the resolved count; identity by conversation; placement on the candidate through the existing line mapping, with the changed-since state; private dispositions; and the return of an item on a new comment. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for item selection, identity and placement, including lines changed and lines removed since the comment. Fixture refreshes that add a comment, resolve a conversation and submit a new review. The owner's look (AC07) is an attended review on a frozen build that opens on an authored pull request with review conversations and a matching worktree.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
