---
id: "RFC-BPR-0122"
title: "Address feedback opens my pull request as a local draft, with each conversation as an item"
status: "Proposed"
metadata: {"origin":"imported 2026-11-11 from the address-review-feedback-v1 pack, where it was RFC-DRAFT-0001","inspected_revision":"b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0","milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"baseline":"91cce126bf211865298f58c79ba86e09bc2b077a"}
depends_on: ["RFC-BPR-0098", "RFC-BPR-0108", "RFC-BPR-0111"]
---

# Address feedback opens my pull request as a local draft, with each conversation as an item

## Problem and grounded current behavior

The owner leaves BetterPullReview to answer a review: "I often leave BPR go
to my agent and ask it to evaluate and respond to/ fix all review comments
on the PR", and wants "that flow fully within BPR as some sort of extension
to the current local draft PR workflow for live PR work"
([note 1](assets/address-review-feedback-v1/SOURCE_NOTES.md)).

The application already knows when that work is waiting. Home gives an
authored pull request the next move **Returned** when "the review decision
is changes requested, or at least one unresolved review conversation's
latest comment is not mine"
([RFC-BPR-0098](RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md)).
The row says so and offers nothing to do about it but open the pull
request, which shows the conversations and has no access to the code that
must change.

The place where the author's code can change is a local PR draft
([RFC-BPR-0108](RFC-BPR-0108-local-pr-drafts.md)). It reaches a live
pull request in one direction only: "If an actual PR is subsequently found
for the same intended branch/destination, offer a verified association". A
pull request that never had a local draft has none, and a local draft shows
the candidate, its notes, evidence, description and tour, but none of the
pull request's review conversations.

## Proposed behavior and boundaries

### Address feedback

A Returned row on Home, and the header of a pull request the signed-in
account authored when it has the facts that make a row Returned, offer
**Address feedback**. It is an ordinary control: it starts no agent.

- When the pull request has an associated local draft, it opens that draft
  on its Feedback area.
- When it has none, it asks for the worktree through the same folder
  chooser as Prepare pull request… and creates a local draft associated
  with the pull request, after verifying both of these: the worktree's
  repository resolves, through RFC-BPR-0108's chain, to the pull request's
  head repository; and the checked-out branch is the pull request's head
  branch.
- A local head equal to the pull request's head, or ahead of it, is
  accepted and shown as such. A local head that is behind or has diverged
  is a specific recovery task that names both commits. BPR moves no branch
  and fetches nothing to make them agree.
- Opening changes nothing in the worktree, as RFC-BPR-0108 already
  requires of opening any local draft.

### Feedback is a working area of the draft

A local draft associated with a pull request gains a **Feedback** area
beside Changes, Evidence, Description and Tour. It lists **feedback items**:

- one for each unresolved review conversation whose latest comment is not
  the author's;
- one for each submitted review that requested changes or commented and
  whose summary has text.

Each item shows who raised it, the conversation as Comments renders it, and
its place in the candidate. Conversations whose latest comment is the
author's are listed apart as waiting on the reviewer, and resolved
conversations are counted, not listed.

An item is identified by its conversation or review, not by its position.
New comments arrive through the existing refresh. A conversation resolved on
GitHub leaves the list. An item that receives a new comment after the author
dealt with it returns to the list marked as having a new reply.

### Placing an item on the candidate

A conversation was attached to lines of some commit of the pull request, and
the candidate may be ahead of it. BPR places the item on the candidate's
lines only when the existing line mapping carries the attached lines to the
candidate unchanged. Otherwise the item says the lines have changed since
the comment and shows them as the reviewer saw them. No place is guessed.
GitHub's outdated mark is shown where GitHub reports one.

### A disposition per item

The author gives an item one **disposition**: Fix, Reply only, Won't change
or Already addressed. It is private preparation, like a note: choosing one
changes nothing on GitHub. An item whose response has been published is
Done.

The no-agent path is complete. The author can set dispositions, edit files
([RFC-BPR-0113](RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md)),
write replies by hand and publish them
([the publishing RFC](RFC-BPR-0124-one-press-publishes-the-response-push-then-replies.md))
without ever starting a run.

### Boundaries

Only pull requests the signed-in account authored offer Address feedback.
Reviewers' text is shown through the existing safe Markdown presentation.
This RFC adds the area and its items; evaluation, fixing and reply drafting
are [the agent RFC](RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md).

Proposals in this RFC that the owner has not separately confirmed are P01
and P02 in the [decision record](assets/address-review-feedback-v1/DECISIONS.md#decisions).

## Acceptance criteria

- **AC01:** A Returned row and the header of an authored pull request with the same facts offer Address feedback; a pull request the account did not author offers none; and pressing it starts no agent.
- **AC02:** With an associated local draft, Address feedback opens it on Feedback. With none, it creates one only when the chosen worktree resolves to the pull request's head repository and has its head branch checked out, and nothing in the worktree changes.
- **AC03:** A local head equal to or ahead of the pull request's head is accepted and labelled; a head behind or diverged is a recovery task naming both commits, and BPR moves no branch and fetches nothing.
- **AC04:** Feedback lists one item per unresolved conversation whose latest comment is not the author's and one per changes-requested or commenting review with summary text; conversations awaiting the reviewer are listed apart; resolved ones are only counted.
- **AC05:** An item is placed on candidate lines only when the line mapping carries its lines there unchanged; otherwise it says the lines changed and shows the reviewer's lines, and no place is guessed.
- **AC06:** A new comment on a dealt-with item returns it to the list marked as having a new reply; a conversation resolved on GitHub leaves the list; and choosing a disposition writes nothing to GitHub.
- **AC07:** On a frozen build serving an authored pull request with review conversations and a matching worktree, the owner accepts Address feedback and the Feedback area.

## Proposed delivery

[Address feedback and the association](../issues/10-backlog/BPR-2105-address-feedback-and-the-pull-request-association.md),
[feedback items and their placement](../issues/10-backlog/BPR-2106-feedback-items-placement-and-dispositions.md)
and [closeout](../issues/10-backlog/BPR-2107-close-rfc-0122.md) form the first
[milestone](plans/address-review-feedback-v1/milestone-plan.json).
