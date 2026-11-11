---
id: "RFC-BPR-0116"
title: "A returning pull request says what changed, and whether a comment was addressed"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the agent-assistance-v1 pack, where it was RFC-DRAFT-0001","inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"baseline":"a9663a706fd72061f60b615bd8e749ee0ddb66b5"}
depends_on: ["RFC-BPR-0009", "RFC-BPR-0094", "RFC-BPR-0101", "RFC-BPR-0102"]
---

# A returning pull request says what changed, and whether a comment was addressed

## Problem and grounded current behavior

When a pull request comes back to Needs me with new commits, the reviewer
re-reads it by hand to answer two questions: what changed since I looked, and
did the author deal with what I raised. The application knows the moment, and
offers Seen and Approve again for it
([BPR-2049](../issues/60-done/BPR-2049-seen-and-approve-again-for-an-approval-with-new-commits.md)),
but offers no help answering either question.

Core has named both questions since the first agent work and no surface asks
them. `agent_entry_point` lists `analyze_latest_upload` and
`evaluate_proposed_fix` among "the eight `docs/ux/ai-experience.md` names"
([agent_chat_scope.hpp](../../src/core/include/bpr/core/agent_chat_scope.hpp)),
and outside core nothing refers to either: every Agent tab send uses
`ask_about_selection`, `workshop_comment` or `ask_about_conversation`.

The nearest existing thing is asking about a conversation
([RFC-BPR-0094](RFC-BPR-0094-ask-the-agent-about-a-conversation.md)),
whose suggested prompts are "Summarize this conversation", "Is the concern
raised here right?" and "Draft a reply to this conversation". None of them
compares the conversation with the commits that followed it.

[RFC-BPR-0009](RFC-BPR-0009-defect-fix-verification-workflow.md) keeps
"author fix claims distinct from reviewer verification" and lists
"automatically claiming a defect is fixed or verified" as a non-goal. The
owner asked for these two entry points to get a surface
([notes 2 and 3](assets/agent-assistance-v1/SOURCE_NOTES.md)).

## Proposed behavior and boundaries

### What changed since my review

Where a pull request has commits newer than the reviewer's last submitted
review or last Seen, the pull request offers **What changed since my
review**: beside Seen and Approve again, and among the Agent tab's
suggestions. It is an agent control with the agent accent of
[RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md).

- A press sends one question through `analyze_latest_upload`. Its subject is
  the pull request, in the sense of
  [RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md),
  and the brief names both revisions: the head the reviewer last reviewed or
  marked Seen, and the current head.
- The answer is an ordinary Agent tab reply: what the new commits changed,
  which of the reviewer's own open conversations they touch, and what it
  could not determine. Its code references are links.
- When the earlier revision's material cannot be read (a rewritten branch
  whose earlier commits are gone, material that was never cached), the
  control is unavailable and says why. A summary of the whole pull request
  is never presented as what changed.
- It changes nothing: no Seen, no attention state, no GitHub write.

### Was this addressed

A review thread the reviewer has commented in offers **Was this addressed?**
when the pull request has commits newer than the reviewer's last comment in
it: among the thread's Ask agent prompts, and as its own control on the
thread.

- A press sends one question through `evaluate_proposed_fix`. Its subject is
  the conversation, exactly as `ask_about_conversation` takes one, and it
  needs no selection. The brief carries the thread, the lines it was
  attached to as they were when the reviewer commented and as they are at
  the current head, and any author reply.
- The answer leads with one of four words: addressed, partly, not addressed
  or cannot tell, and then the evidence with code references. It is shown as
  the agent's claim.
- It resolves no thread, changes no defect's disposition under RFC-BPR-0009
  and posts nothing. Drafting a reply from it is the existing explicit
  action.

Core today says `evaluate_proposed_fix` "acts on what the reviewer has
selected". This RFC changes that rule: it acts on a conversation. The same
entry point serves Check the fix on a local draft
([RFC-BPR-0115](RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md)).

### Each has its own prompt

Each of the two operations is briefed by a built-in prompt of its own in
Settings › Agents & context › Built-in prompts, with the editor, default,
version and Reset the other built-in prompts have, and stable purpose IDs
`review.latest-upload` and `review.proposed-fix`. Editing one changes no
other prompt. This follows the owner's request for self-review
([note 4](assets/agent-assistance-v1/SOURCE_NOTES.md)) and is a proposal here; see P02 in the
[decision record](assets/agent-assistance-v1/DECISIONS.md#decisions).

### Boundaries

Nothing starts either run but the reviewer's press. Opening, refreshing or
returning to a pull request launches no agent. Both are read-only runs under
the grants the Agent tab already has.

## Acceptance criteria

- **AC01:** What changed since my review is offered only when the pull request has commits newer than the reviewer's last review or Seen; a press sends `analyze_latest_upload` with both revisions named in the brief; and the reply's code references open as links.
- **AC02:** When the earlier revision's material cannot be read, the control is unavailable with that reason, and no whole-pull-request answer is labelled as what changed.
- **AC03:** Was this addressed? on a thread sends `evaluate_proposed_fix` with the conversation as its subject, with no selection required, and its brief carries the attached lines as they were and as they are.
- **AC04:** The answer leads with addressed, partly, not addressed or cannot tell and is shown as the agent's claim; the thread is not resolved, no defect's disposition changes and nothing is posted.
- **AC05:** Each operation is briefed by its own built-in prompt with its own editor, default, version and Reset; an edit reaches that operation's next brief and no other prompt.
- **AC06:** Opening, refreshing and returning to a pull request with new commits launches no agent, and neither operation writes to GitHub or changes attention state.
- **AC07:** On a frozen build serving a pull request with commits newer than a review and a commented thread, the owner accepts both controls and their answers.

## Proposed delivery

[What changed since my review](../issues/10-backlog/BPR-2073-what-changed-since-my-review.md),
[Was this addressed?](../issues/10-backlog/BPR-2074-was-this-addressed-on-a-thread.md) and
[closeout](../issues/10-backlog/BPR-2075-close-rfc-0116.md) form the first
[milestone](plans/agent-assistance-v1/milestone-plan.json).
