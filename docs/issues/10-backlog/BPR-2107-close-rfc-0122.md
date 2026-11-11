---
schema: "implement-rfc.card.v1"
id: "BPR-2107"
title: "Verify and close RFC-BPR-0122"
status: "Backlog"
rfcs: ["RFC-BPR-0122"]
depends_on: ["BPR-2105", "BPR-2106"]
closeout_for: ["RFC-BPR-0122"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0003-close-address-feedback.md","draft_title":"Close Address feedback and the Feedback area"}
---

# BPR-2107: Verify and close RFC-BPR-0122

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0122](../../rfcs/RFC-BPR-0122-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for Address feedback and feedback items.

## Intended verification

Judge every criterion on the composed application, and once against a real authored pull request on `MikeMarcin/BPR-Test` reviewed by the test account.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `91cce126bf211865298f58c79ba86e09bc2b077a`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** A Returned row and the header of an authored pull request with the same facts offer Address feedback; a pull request the account did not author offers none; and pressing it starts no agent. | Pending | Pending | Not implemented |
| **AC02:** With an associated local draft, Address feedback opens it on Feedback. With none, it creates one only when the chosen worktree resolves to the pull request's head repository and has its head branch checked out, and nothing in the worktree changes. | Pending | Pending | Not implemented |
| **AC03:** A local head equal to or ahead of the pull request's head is accepted and labelled; a head behind or diverged is a recovery task naming both commits, and BPR moves no branch and fetches nothing. | Pending | Pending | Not implemented |
| **AC04:** Feedback lists one item per unresolved conversation whose latest comment is not the author's and one per changes-requested or commenting review with summary text; conversations awaiting the reviewer are listed apart; resolved ones are only counted. | Pending | Pending | Not implemented |
| **AC05:** An item is placed on candidate lines only when the line mapping carries its lines there unchanged; otherwise it says the lines changed and shows the reviewer's lines, and no place is guessed. | Pending | Pending | Not implemented |
| **AC06:** A new comment on a dealt-with item returns it to the list marked as having a new reply; a conversation resolved on GitHub leaves the list; and choosing a disposition writes nothing to GitHub. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build serving an authored pull request with review conversations and a matching worktree, the owner accepts Address feedback and the Feedback area. | Pending | Pending | Not implemented |
