# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **address-review-feedback-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## feedback-items

From a Returned pull request the author opens a local draft on their worktree and sees every review conversation as an item with a place and a disposition.

**Work:** `BPR-2105`, `BPR-2106`, `BPR-2107`

**Prerequisites:** None

**Completion:**

- Address feedback opens or creates the associated local draft only on a verified worktree, and changes nothing in it.
- Items come from unresolved conversations and reviews with summaries, are identified by conversation, and are placed on candidate lines only when the mapping carries them unchanged.
- Dispositions are private, and the whole area is usable with no agent.

**Boundary:** Reading and organising only: no agent run and no publication. Needs the local draft shell and pull request association of RFC-BPR-0108 and RFC-BPR-0111 first.

## agent-feedback-work

The author has an agent evaluate every item, fix the ones they accept in one run, and draft the replies.

**Work:** `BPR-2108`, `BPR-2109`, `BPR-2110`, `BPR-2111`

**Prerequisites:** feedback-items

**Completion:**

- Evaluation proposes and the author disposes; nothing is accepted automatically.
- Reviewers' text is quoted data in every brief and cannot change what is run or granted.
- One change run fixes the accepted items without committing, with changes kept or reverted per file and attributed to items as a claim.
- Replies are drafts; a reply naming a commit not on the branch is refused at publication.
- Evaluation and reply drafting each have their own built-in prompt.

**Boundary:** Nothing is posted, resolved or marked Done by an agent. Needs RFC-BPR-0115's change runs and RFC-BPR-0110's local-draft sessions first.

## publish-response

One press pushes the commits and then posts the replies, and Home stops calling the pull request the author's move once the reviewers have been asked again.

**Work:** `BPR-2112`, `BPR-2113`, `BPR-2114`

**Prerequisites:** feedback-items

**Completion:**

- The plan is exact and finite; push precedes replies; a failed push posts nothing.
- A partial failure resumes only the remainder and never posts a reply twice.
- Nothing is resolved or requested that the author did not tick.
- A changes-requested pull request reads Waiting once every requesting reviewer has been asked again and no conversation awaits the author.
- RFC-BPR-0098's live text is reconciled with that change.

**Boundary:** No commit, force push, merge or approval action. Needs RFC-BPR-0114's Push first. Usable with replies written by hand, so it does not wait on agent-feedback-work.
