# Address review feedback: proposed pack

The author of a pull request answers a review without leaving
BetterPullReview: open the returned pull request on their worktree, see
every conversation as an item, have an agent evaluate them, fix the ones
they accept and draft the replies, then publish the push and the replies in
order with one press. It extends the local PR draft (RFC-BPR-0108 to 0115)
to a pull request that is already live. Nothing here has been implemented,
and nothing has been imported: the IDs are provisional.

The owner's words are in [SOURCE_NOTES.md](SOURCE_NOTES.md).
[DECISIONS.md](DECISIONS.md) is the single record of what the owner said,
what is only proposed, the deferrals, the checks run on this draft and the
unevaluated execution eligibility. [AMENDMENTS.md](AMENDMENTS.md) lists what
this changes in adopted RFCs.

## The RFCs

| RFC | Outcome |
|---|---|
| [RFC-DRAFT-0001](RFC-DRAFT-0001-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md) | Address feedback opens the pull request as a local draft on the author's worktree, with a Feedback area holding one item per conversation |
| [RFC-DRAFT-0002](RFC-DRAFT-0002-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md) | An agent evaluates the items, fixes the accepted ones in one run and drafts the replies |
| [RFC-DRAFT-0003](RFC-DRAFT-0003-one-press-publishes-the-response-push-then-replies.md) | Publish response pushes, then posts the replies; Home reads Waiting once reviewers are asked again |

The rules of the earlier packs hold here too. Nothing starts a run but a
press. An agent proposes and the author disposes. Whatever changed the
worktree comes back to be kept or reverted. Nothing reaches GitHub until the
author publishes, and nothing is resolved that the author did not tick.

## Cards and plan

Ten proposed cards are in [cards/](cards/). The canonical grouping is
[milestone-plan.json](milestone-plan.json), with its
[schema](milestone-plan.schema.json); [MILESTONES.md](MILESTONES.md) is
generated from it. `feedback-items` comes first. `agent-feedback-work` and
`publish-response` both follow it and are independent of each other: the
flow is complete by hand without an agent.

Start from the [implementation entrypoint](IMPLEMENTATION_PROMPT.md).
