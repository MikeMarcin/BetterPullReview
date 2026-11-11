---
schema: "implement-rfc.card.v1"
id: "BPR-2113"
title: "Home reads Waiting after a response"
status: "Backlog"
rfcs: ["RFC-BPR-0124"]
depends_on: ["BPR-2112"]
metadata: {"criteria":["AC07"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0009-home-reads-waiting-after-a-response.md"}
---

# Home reads Waiting after a response

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0124](../../rfcs/RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md), criteria `AC07`.

The one change to RFC-BPR-0098's next move: a changes-requested pull request reads Waiting when every reviewer who requested changes has been asked again since their review and no conversation awaits the author; the row's sentence; and the bounded facts Home needs to decide it. Reconciles RFC-BPR-0098's live text under the pack's amendment.

## Intended verification

Core tests in the shape of `core_review_queue_attention`: asked again, asked before the review, one of two reviewers asked, and a conversation still awaiting the author.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).

## Ask before building

This card builds P09 of the pack's
[decision record](../../rfcs/assets/address-review-feedback-v1/DECISIONS.md#decisions),
which changes an adopted rule of RFC-BPR-0098 (amendment A1 in
[AMENDMENTS.md](../../rfcs/assets/address-review-feedback-v1/AMENDMENTS.md)).
The owner has not seen it. Put it to the owner as an Accept or Reject
question before the card is readied; the import did not apply the amendment.
