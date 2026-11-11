---
id: "RFC-DRAFT-0002"
title: "Explain this file where I am reading"
status: "Proposed"
metadata: {"inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: ["RFC-BPR-0101", "RFC-BPR-0102"]
---

# Explain this file where I am reading

## Problem and grounded current behavior

Asking an agent about the file in front of the reviewer means leaving Review
for the Agent tab.
[RFC-BPR-0102](../../RFC-BPR-0102-agent-questions-say-what-they-point-at.md)
gives a question a file as its subject only when "a suggestion names the
open file", and suggestions live in the Agent tab.

Core names the action, `analyze_current_file`
([agent_chat_scope.hpp](../../../../src/core/include/bpr/core/agent_chat_scope.hpp)),
and no surface calls it. Its rule there is that it acts on the selection,
because "the selection is the only input that names a file". The open file
in Review names one too.

The owner asked for this entry point to get a surface, "without a trip to
the Agent tab" ([notes 2 and 3](SOURCE_NOTES.md)).

## Proposed behavior and boundaries

A file's header in Review, and its context menu, offer **Explain this
file**, an agent control.

- A press sends one question through `analyze_current_file`. Its subject is
  that file, the rest of the change is context, and no selection is needed.
  Core's rule changes accordingly: the entry point acts on the open file.
- The answer appears in Review, in a card at the file's header: what the
  file's changes do, how they fit the rest of the pull request, and what to
  look at. Its code references are links. While the run is under way the
  card shows the turn's activity as
  [RFC-BPR-0103](../../RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
  defines.
- The card offers **Keep as note**, which makes a private file-level note
  with its agent origin; **Continue in Agent tab**; and **Dismiss**. A
  dismissed answer is still a turn in the Agent tab's history, because the
  question is a turn in the pull request's ordinary agent session.
- It is briefed by a built-in prompt of its own, `review.explain-file`,
  with the editor, default, version and Reset of the other built-in prompts
  (a proposal; P02 in the [decision record](DECISIONS.md#decisions)).
- Nothing starts it but the press. Opening or scrolling to a file launches
  no agent, and nothing is written to GitHub.

Explaining a selection of lines in place, rather than a file, is not in this
RFC; the Agent tab's attached lines cover it today.

## Acceptance

- **AC01:** Explain this file on a file's header and context menu sends `analyze_current_file` with that file as the subject and no selection, and the turn is in the pull request's Agent tab history.
- **AC02:** The answer shows in a card at the file's header in Review with its references as links, and a running turn shows its activity there.
- **AC03:** Keep as note makes a private file-level note marked with its agent origin; Dismiss removes the card and no note; Continue in Agent tab opens that turn.
- **AC04:** The operation is briefed by its own built-in prompt, whose edit reaches the next Explain this file and no other prompt.
- **AC05:** Opening, scrolling to or accepting a file launches no agent, and the operation writes nothing to GitHub.
- **AC06:** On a frozen build with a fixture agent, the owner accepts the control and the answer card.

## Proposed delivery

[Explain this file](cards/DRAFT-0004-explain-this-file-in-review.md) and
[closeout](cards/DRAFT-0005-close-explain-this-file.md) form the second
[milestone](milestone-plan.json).
