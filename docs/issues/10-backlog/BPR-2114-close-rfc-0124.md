---
schema: "implement-rfc.card.v1"
id: "BPR-2114"
title: "Verify and close RFC-BPR-0124"
status: "Backlog"
rfcs: ["RFC-BPR-0124"]
depends_on: ["BPR-2112", "BPR-2113"]
closeout_for: ["RFC-BPR-0124"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0010-close-publish-response.md","draft_title":"Close Publish response"}
---

# BPR-2114: Verify and close RFC-BPR-0124

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0124](../../rfcs/RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

Acceptance assessment for publishing a response and for Home afterwards.

## Intended verification

Judge every criterion on the composed application, including the live course's result as GitHub's own web interface shows it.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `91cce126bf211865298f58c79ba86e09bc2b077a`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Publish response… previews the exact commits, replies, ticked resolutions and review requests, and a change to any of them after the preview needs a new press. | Pending | Pending | Not implemented |
| **AC02:** With unpushed commits the push runs first and the replies are posted only after the hosted head is observed equal to the local head; a failed or refused push posts no reply. | Pending | Pending | Not implemented |
| **AC03:** Uncommitted kept changes block the plan with that reason, and Publish response makes no commit. | Pending | Pending | Not implemented |
| **AC04:** A reply that fails leaves earlier replies posted and later ones unsent; resuming sends only the remainder; and after a lost answer nothing is sent again before GitHub is read, so no reply is posted twice. | Pending | Pending | Not implemented |
| **AC05:** No conversation is resolved unless the author ticked it, review is requested only when ticked, and no agent run changes either. | Pending | Pending | Not implemented |
| **AC06:** An item whose reply was posted is Done and returns only when the reviewer comments again. | Pending | Pending | Not implemented |
| **AC07:** A changes-requested pull request whose requesting reviewers have all been asked again since their reviews, with no conversation awaiting the author, reads Waiting on Home with a sentence naming who was asked; with one of those conditions missing it still reads Returned. | Pending | Pending | Not implemented |
| **AC08:** A review submission for another pull request completes while a response is being published, and each step shows as waiting, running, done, failed or not run. | Pending | Pending | Not implemented |
| **AC09:** On a frozen build, the owner accepts the Publish response preview, a completed response and a failed push. | Pending | Pending | Not implemented |
