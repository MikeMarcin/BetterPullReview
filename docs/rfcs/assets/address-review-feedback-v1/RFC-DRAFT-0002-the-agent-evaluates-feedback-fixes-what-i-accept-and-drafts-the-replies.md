---
id: "RFC-DRAFT-0002"
title: "The agent evaluates feedback, fixes what I accept and drafts the replies"
status: "Proposed"
metadata: {"inspected_revision":"b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0","milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/assets/address-review-feedback-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: ["RFC-BPR-0101", "RFC-BPR-0103", "RFC-BPR-0107", "RFC-BPR-0110", "RFC-BPR-0115", "RFC-DRAFT-0001"]
---

# The agent evaluates feedback, fixes what I accept and drafts the replies

## Problem and grounded current behavior

What the owner asks an outside agent to do is three things in one breath:
"evaluate and respond to/ fix all review comments on the PR"
([note 1](SOURCE_NOTES.md)). The pieces the application has, or has
planned, each do part of one of them for one item at a time:

- [RFC-BPR-0115](../../RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md)
  gives an agent a change run in the author's worktree, started by Fix "on a
  note of the local draft", with Keep and Revert per changed file and Check
  the fix. It has no notion of a reviewer's conversation and no run over
  many items.
- Asking about a conversation
  ([RFC-BPR-0094](../../RFC-BPR-0094-ask-the-agent-about-a-conversation.md))
  can "Draft a reply to this conversation", one conversation at a time, in
  a pull request review that cannot change code.
- A local draft's conversations read the captured candidate and are
  read-only
  ([RFC-BPR-0110](../../RFC-BPR-0110-local-tours-and-description.md)).

The [feedback RFC](RFC-DRAFT-0001-address-feedback-opens-my-pull-request-with-each-conversation-as-an-item.md)
puts every conversation in the draft as an item with a disposition. This RFC
lets an agent do the reading, the fixing and the first draft of each reply
across all of them.

## Proposed behavior and boundaries

All three operations are agent controls with the agent accent
([RFC-BPR-0101](../../RFC-BPR-0101-agent-chat-conventions.md)). Each acts on
every eligible item, or on the items the author selected. None starts
without a press, and none writes to GitHub.

A reviewer's words are someone else's text. In every brief the
conversations are passed as quoted data, marked as such, never as
instructions to the agent.

### Evaluate feedback

**Evaluate feedback** is one read-only run on the candidate workspace, of
the kind RFC-BPR-0110 defines. For each open item it returns:

- a proposed disposition: Fix, Reply only, Won't change, Already addressed,
  or **Needs your answer** when only the author can answer it;
- its reason, with code references that open as links;
- for Fix, a short plan.

Each proposal is shown on its item as the agent's. The author accepts it or
chooses another disposition, item by item or for all proposals at once.
Nothing is accepted automatically. An item the run returned nothing for is
shown as not evaluated.

### Fix accepted items

**Fix accepted items** is one change run under RFC-BPR-0115's grant, for the
items whose disposition is Fix. It follows that RFC's fix-run rules: it
changes files and makes no commit, and its changes come back to be kept or
reverted file by file.

- The agent reports, per item, the files it changed for it and what it did,
  or that it could not and why.
- The run's changes show beside each file the items the agent attributes it
  to, as the agent's claim. A changed file attributed to no item is marked
  as such.
- An item's own **Fix** runs the same thing for that one item.
- **Check the fix** is offered per item on the successor candidate, as
  RFC-BPR-0115 defines, and for all fixed items at once.

### Draft replies

**Draft replies** writes a draft reply for each item that has a disposition
and no reply yet. It is a read-only run. Each draft is an ordinary draft
reply on its conversation: the author edits, keeps or discards it, and
Enhance works on it as on any draft.

- For Fix it says what changed. For Won't change and Already addressed it
  says why. For Reply only it answers. An item that Needs your answer gets
  no draft.
- A reply drafted after the fix is committed may name that commit. Core
  checks every commit a draft reply names: it must be on the draft's branch.
  Publication refuses a reply naming a commit that is not, and says which.
- A reply to a review's summary is drafted as a comment on the pull
  request, since a summary has no conversation to reply in.

### Prompts

Evaluation and reply drafting are each briefed by a built-in prompt of
their own in Settings › Agents & context › Built-in prompts, with the
editor, default, version and Reset the others have, and the stable purpose
IDs `local-draft.feedback-evaluate` and `local-draft.feedback-reply`,
recognized by the sharing format of
[RFC-BPR-0107](../../RFC-BPR-0107-agent-and-prompt-sharing.md). The fix run
is briefed as RFC-BPR-0115's fix runs are.

### Boundaries

- Runs show their work and end as
  [RFC-BPR-0103](../../RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
  defines.
- While the change run is active, the exclusions of RFC-BPR-0115 apply.
- The agent resolves nothing, posts nothing and marks no item Done.

Proposals in this RFC that the owner has not separately confirmed are P03
to P06 in the [decision record](DECISIONS.md#decisions).

## Acceptance

- **AC01:** Evaluate feedback is a read-only run over the open or selected items, and each item then shows the agent's proposed disposition, reason and, for Fix, plan, marked as the agent's; no disposition changes until the author accepts or chooses one; an item with no returned verdict shows as not evaluated.
- **AC02:** In every brief of the three operations the reviewers' text is carried as marked quoted data, and a conversation containing an instruction to the agent changes neither what is run nor what is granted.
- **AC03:** Fix accepted items is one change run under the Edits this worktree grant over the items whose disposition is Fix; it makes no commit; and its changes are kept or reverted per file as RFC-BPR-0115 defines.
- **AC04:** The run's changes show the items the agent attributes each file to, as a claim, and mark a changed file attributed to none; each item shows changed, or could not with the reason.
- **AC05:** Draft replies writes one draft reply per item with a disposition and no reply, none for Needs your answer, as editable drafts on their conversations, and a review summary's reply as a pull request comment draft; nothing is posted.
- **AC06:** A draft reply naming a commit that is not on the draft's branch is refused at publication with the commit named, and one naming a commit that is on it is not.
- **AC07:** Evaluation and reply drafting each have their own built-in prompt; an edit to one reaches that operation's next brief and no other prompt.
- **AC08:** None of the three operations starts without a press, resolves a conversation, posts to GitHub or marks an item Done.
- **AC09:** On a frozen build with a fixture agent, the owner accepts the evaluated items, the batch fix's changes with their attribution, and the drafted replies.

## Proposed delivery

[Evaluate feedback](cards/DRAFT-0004-evaluate-feedback.md),
[Fix accepted items](cards/DRAFT-0005-fix-accepted-items-in-one-run.md),
[Draft replies](cards/DRAFT-0006-draft-replies-and-the-commit-reference-check.md)
and [closeout](cards/DRAFT-0007-close-agent-feedback-work.md) form the
second [milestone](milestone-plan.json).
