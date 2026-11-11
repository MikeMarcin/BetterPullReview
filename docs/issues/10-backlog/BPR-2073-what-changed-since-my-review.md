---
schema: "implement-rfc.card.v1"
id: "BPR-2073"
title: "What changed since my review"
status: "Backlog"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC01","AC02","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0001-what-changed-since-my-review.md"}
depends_on: []
---

# What changed since my review

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC01`, `AC02`, `AC05`, `AC06`.

The control beside Seen and Approve again and among the Agent tab's suggestions; the `analyze_latest_upload` send with both revisions named; the unavailable state when the earlier revision cannot be read; and its own built-in prompt, `review.latest-upload`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for when the control is offered and for the two revisions it names. The composed path on a fixture pull request with commits newer than a review, and on one whose earlier commits are gone. The launch inventory shows no agent started by opening or refreshing.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).
