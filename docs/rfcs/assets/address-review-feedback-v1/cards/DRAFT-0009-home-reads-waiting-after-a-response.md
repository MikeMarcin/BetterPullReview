---
schema: "implement-rfc.card.v1"
id: "DRAFT-0009"
title: "Home reads Waiting after a response"
status: "Backlog"
rfcs: ["RFC-DRAFT-0003"]
depends_on: ["DRAFT-0008"]
metadata: {"criteria": ["AC07"], "milestone_plan": {"id": "address-review-feedback-v1", "path": "docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"}}
---

# Home reads Waiting after a response

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0003](../RFC-DRAFT-0003-one-press-publishes-the-response-push-then-replies.md), criteria `AC07`.

The one change to RFC-BPR-0098's next move: a changes-requested pull request reads Waiting when every reviewer who requested changes has been asked again since their review and no conversation awaits the author; the row's sentence; and the bounded facts Home needs to decide it. Reconciles RFC-BPR-0098's live text under the pack's amendment.

## Intended verification

Core tests in the shape of `core_review_queue_attention`: asked again, asked before the review, one of two reviewers asked, and a conversation still awaiting the author.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
