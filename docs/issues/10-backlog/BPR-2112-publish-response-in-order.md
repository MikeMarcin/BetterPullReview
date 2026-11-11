---
schema: "implement-rfc.card.v1"
id: "BPR-2112"
title: "Publish response, in order"
status: "Backlog"
rfcs: ["RFC-BPR-0124"]
depends_on: ["BPR-2106", "BPR-2066"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC08","AC09"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0008-publish-response-in-order.md"}
---

# Publish response, in order

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0124](../../rfcs/RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC08`, `AC09`.

The preview as an exact finite plan; push first under RFC-BPR-0114, observation of the hosted head, then replies, ticked resolutions and ticked review requests; the stop on a failed push; resume of only the remainder; read-before-resend after a lost answer; Done items; and RFC-BPR-0099's reporting. The implementation records whether replies reach GitHub singly or as one review. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

A local bare repository and the GitHub fixture cover the order, a refused push, a reply that fails midway and a lost answer. One live course on `MikeMarcin/BPR-Test`: the test account reviews, the owner account pushes a fix and publishes replies, and the course closes what it made. The owner's look (AC09) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).
