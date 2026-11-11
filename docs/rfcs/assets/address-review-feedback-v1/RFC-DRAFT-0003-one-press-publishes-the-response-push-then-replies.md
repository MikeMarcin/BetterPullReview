---
id: "RFC-DRAFT-0003"
title: "One press publishes the response: push, then replies"
status: "Proposed"
metadata: {"inspected_revision":"b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0","milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: ["RFC-BPR-0069", "RFC-BPR-0091", "RFC-BPR-0098", "RFC-BPR-0099", "RFC-BPR-0105", "RFC-BPR-0111", "RFC-BPR-0114", "RFC-DRAFT-0001"]
---

# One press publishes the response: push, then replies

## Problem and grounded current behavior

Answering a review ends in two publications that must happen in order: the
commits, then the replies that describe them. A reply that says "fixed"
before the fix is on GitHub is wrong for as long as the push takes, and
wrong for good if the push fails.

The parts exist separately.
[RFC-BPR-0114](../../RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md)
pushes an associated local draft's later commits on the author's press.
A draft reply to a conversation is already a draft with its own publication
(`ReplyToThread`,
[review_round_trip.hpp](../../../../src/core/include/bpr/core/review_round_trip.hpp)). Resolving a
conversation is its own action
([RFC-BPR-0091](../../RFC-BPR-0091-comment-reactions-and-resolve-conversation.md)),
and requesting a reviewer is an existing operation
([RFC-BPR-0069](../../RFC-BPR-0069-reviewers-and-merge-requirements.md)).
Nothing joins them, and
[RFC-BPR-0111](../../RFC-BPR-0111-draft-publication-and-continuity.md) is
explicit that there are no "automatic post-publication pushes".

After the author has answered, Home still calls the pull request theirs to
act on. A changes-requested decision stays on GitHub until that reviewer
reviews again, and
[RFC-BPR-0098](../../RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md)
makes "the review decision is changes requested" a Returned row whatever
the author has done since.

## Proposed behavior and boundaries

### Publish response

The Feedback area offers **Publish response…** when there is something to
send: unpushed commits, or kept draft replies. It is an ordinary control.

Its preview is the exact, finite plan, in the sense RFC-BPR-0111 gives a
publication preview:

- the commits that will be pushed, and to which branch;
- each reply that will be posted, with its conversation;
- the conversations that will be resolved: none unless the author ticks
  them, one by one;
- **Re-request review**, unticked, naming the reviewers it would ask.

One press authorizes that plan and no other. A new commit, an edited reply
or a new comment from GitHub changes the plan and needs a new press.

### Order

1. **Push**, when there are unpushed commits, exactly as RFC-BPR-0114
   defines Push. Kept changes that are not committed block the plan with
   that reason; Publish response makes no commit.
2. **Observe** that the hosted head equals the local head.
3. **Post the replies**, as the signed-in account, through the existing
   reply publication path.
4. **Resolve** the ticked conversations.
5. **Request review** from the named reviewers, when ticked.

A failed or refused push stops the plan: no reply is posted. A reply that
fails leaves the earlier ones posted and the later ones not sent, and
resuming sends only what remains. After a lost answer BPR reads GitHub
before sending anything again, and never posts a reply twice. With no
commits to push, the plan starts at the replies.

Each step reports as
[RFC-BPR-0099](../../RFC-BPR-0099-tour-publishing-in-reach-and-reported.md)
defines for tour publication, and no other work waits on it, as
[RFC-BPR-0105](../../RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md)
requires.

### After publishing

An item whose reply was posted becomes Done. It returns as an item only if
the reviewer comments again.

Home's next move changes in one respect. A pull request whose review
decision is changes requested reads **Waiting**, not Returned, when every
reviewer whose review requested changes has been asked to review again
since that review, and no unresolved conversation's latest comment is
someone else's. The row's sentence says who was asked. Everything else in
RFC-BPR-0098's table is unchanged, and the row is still decided from facts
GitHub reports.

### Boundaries

- Nothing here is automatic. Keeping a fix, committing it and drafting a
  reply publish nothing.
- No conversation is resolved that the author did not tick. An agent ticks
  nothing.
- No force push, as RFC-BPR-0114 requires; no ready-for-review, merge or
  approval action.
- Whether the replies reach GitHub one by one or as one review is whatever
  the existing reply path does; a reviewer sees ordinary GitHub replies
  either way. The implementation records which.

Proposals in this RFC that the owner has not separately confirmed are P07
to P09 in the [decision record](DECISIONS.md#decisions).

## Acceptance

- **AC01:** Publish response… previews the exact commits, replies, ticked resolutions and review requests, and a change to any of them after the preview needs a new press.
- **AC02:** With unpushed commits the push runs first and the replies are posted only after the hosted head is observed equal to the local head; a failed or refused push posts no reply.
- **AC03:** Uncommitted kept changes block the plan with that reason, and Publish response makes no commit.
- **AC04:** A reply that fails leaves earlier replies posted and later ones unsent; resuming sends only the remainder; and after a lost answer nothing is sent again before GitHub is read, so no reply is posted twice.
- **AC05:** No conversation is resolved unless the author ticked it, review is requested only when ticked, and no agent run changes either.
- **AC06:** An item whose reply was posted is Done and returns only when the reviewer comments again.
- **AC07:** A changes-requested pull request whose requesting reviewers have all been asked again since their reviews, with no conversation awaiting the author, reads Waiting on Home with a sentence naming who was asked; with one of those conditions missing it still reads Returned.
- **AC08:** A review submission for another pull request completes while a response is being published, and each step shows as waiting, running, done, failed or not run.
- **AC09:** On a frozen build, the owner accepts the Publish response preview, a completed response and a failed push.

## Proposed delivery

[Publish response](cards/DRAFT-0008-publish-response-in-order.md),
[Home after a response](cards/DRAFT-0009-home-reads-waiting-after-a-response.md)
and [closeout](cards/DRAFT-0010-close-publish-response.md) form the third
[milestone](milestone-plan.json).
