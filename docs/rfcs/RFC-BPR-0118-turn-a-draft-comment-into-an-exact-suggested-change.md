---
id: "RFC-BPR-0118"
title: "Turn a draft comment into an exact suggested change"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the agent-assistance-v1 pack, where it was RFC-DRAFT-0003","inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"baseline":"a9663a706fd72061f60b615bd8e749ee0ddb66b5"}
depends_on: ["RFC-BPR-0090", "RFC-BPR-0101"]
---

# Turn a draft comment into an exact suggested change

## Problem and grounded current behavior

A reviewer who writes "this should check for null first" often means a
specific edit, and writing that edit is the slow part.
[RFC-BPR-0090](RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md)
lets the reviewer type a suggestion into the code pane, and nothing writes
one for them. The agent's comment workshop has six operations (clarify,
shorten, make actionable, phrase as a question, make more direct, check the
premise;
[agent_chat_scope.hpp](../../src/core/include/bpr/core/agent_chat_scope.hpp)),
all of which rewrite prose. An agent's suggested comments are prose too.

A suggestion is the one agent output whose form can be checked exactly: it
replaces named lines with other lines. The owner asked for it
([notes 2 and 3](assets/agent-assistance-v1/SOURCE_NOTES.md)).

## Proposed behavior and boundaries

A draft comment anchored to head lines that GitHub can take a suggestion on,
under RFC-BPR-0090's range rules, offers **Suggest the change**, an agent
control.

- A press asks the agent, through the comment workshop entry point with a
  seventh operation, for replacement text for exactly the draft's lines. The
  brief carries the comment's text, those lines and their surroundings.
- The reply is taken only as a structured replacement: the range, the text
  the agent says is there now, and the text to put in its place.
- Core accepts it only when all of these hold: the range is the draft's
  range at the current head; the text the agent says is there equals the
  head's lines byte for byte; the replacement differs from it, is valid
  UTF-8 and is within RFC-BPR-0090's bounds; and the result is a change
  GitHub can take as a suggestion under that RFC.
- An accepted replacement opens as an in-pane suggestion edit exactly as
  RFC-BPR-0090 defines one, with "What it changes" showing it, unsaved. The
  reviewer edits, keeps or discards it. Keeping it puts the suggestion block
  into the draft comment; the comment's own text is left as written.
- A reply that fails any check is not shown as a suggestion. The reviewer
  is told that the agent's change did not apply to these lines and which
  check failed, and can read the reply as plain text.
- A new head that moves the lines while the run is under way fails the
  first check; nothing is re-anchored by guesswork.
- One Undo restores the draft as it was. Nothing is sent to GitHub: the
  draft is still a draft.
- It is briefed by a built-in prompt of its own, `review.suggest-change`
  (a proposal; P02 in the [decision record](assets/agent-assistance-v1/DECISIONS.md#decisions)).

Suggesting a change for a finding card that has not been made a draft, for
several comments at once, or spanning files is not in this RFC.

## Acceptance criteria

- **AC01:** Suggest the change is offered only on a draft comment whose lines can take a suggestion under RFC-BPR-0090's range rules, and a press sends the comment workshop entry point with the new operation and exactly those lines.
- **AC02:** A reply whose stated current text equals the head's lines opens as an in-pane suggestion edit showing the agent's replacement, and the draft comment is unchanged until the reviewer keeps it.
- **AC03:** A reply whose range differs, whose stated current text differs by any byte, whose replacement is identical or out of bounds, or which GitHub could not take as a suggestion is refused with that reason and never shown as a suggestion; a head that moved the lines during the run is such a refusal.
- **AC04:** Keeping the suggestion adds the suggestion block to the draft and leaves its text as written; one Undo restores the draft; nothing is sent to GitHub.
- **AC05:** The operation is briefed by its own built-in prompt, whose edit reaches the next Suggest the change and no other prompt.
- **AC06:** On a frozen build with a fixture agent, the owner accepts the control, an applied suggestion and a refused one.

## Proposed delivery

[Suggest the change](../issues/10-backlog/BPR-2078-suggest-the-change-from-a-draft-comment.md)
and [closeout](../issues/10-backlog/BPR-2079-close-rfc-0118.md) form the third
[milestone](plans/agent-assistance-v1/milestone-plan.json).
