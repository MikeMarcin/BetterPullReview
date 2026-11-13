---
id: "RFC-BPR-0118"
title: "Turn a draft comment into an exact suggested change"
status: "In Progress"
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
  (P02, settled by the owner on 2026-11-12, in the [decision record](assets/agent-assistance-v1/DECISIONS.md#decisions)).

Suggesting a change for a finding card that has not been made a draft, for
several comments at once, or spanning files is not in this RFC.

### Owner decisions of 2026-11-12

- P02 is settled: this action has one editable prompt of its own. Its
  instructions reach Claude as an inline `--append-system-prompt` and Codex
  `exec` as `developer_instructions`, pasted where unverified or switched off
  (RFC-BPR-0107's amendment of 2026-11-12).
- Its first answer is a fresh run (RFC-BPR-0101's amendment of 2026-11-12):
  no native session, nothing saved. It is one model call with no tools: the context it needs is in the request. Ask more on the answer promotes it into a
  resumed session whose first turn carries the answer.

### Coordinator redirect of 2026-11-12 (recorded, verbatim intent)

The owner redirected the agent-assistance entry points toward one "Ask"
model, details to come in a later round: an Ask menu at pull request scope,
Ask on a file and Ask on a thread; suggested prompts are macros with a
"Quick ask" checkbox in the macro editor; quick asks show in the Ask menu,
filtered by context (an invalid one greyed or hidden); answers go to the
Agent tab, some opening in a larger lightbox-style reader. For this round the
coordinator directed: build the action's core, host and prompt; keep its Qt
entry minimal (the draft comment's right-click menu) and register it as a
macro or quick-ask candidate through RFC-BPR-0125's milestone 2 system; no
new visible control and no owner look for its placement.

Consequence for AC06: the owner's look at "the control" waits for the Ask
model. The applied and the refused suggestion can be looked at on the
right-click entry; whether to file that look now is the coordinator's.

### Implementation notes (2026-11-12)

- Core (`bpr/core/suggest_change.hpp`): the material (the draft's lines at the
  head without the last one's ending, its words, at most twelve lines each side),
  the structured reply (one JSON object, bare or in one fenced block:
  `first_line`, `last_line`, `current`, `replacement`) and
  `decide_suggested_change`, whose checks run in this order: the reply reads,
  the head the press read is still the head with the same lines there
  (`head_moved`), the range is the draft's, `current` equals the lines byte
  for byte, the replacement is text within 64 KiB and differs, and RFC-BPR-0090's
  plan of it is placeable and reads back to the same block. A single final
  line ending in the replacement is not a line; in a CRLF range a CR before a
  break is the line's ending. An empty replacement deletes the lines.
- `workshop_operation::suggest_change` is the seventh operation; it is not a
  prose rewrite, so the comment workshop's six controls and the review
  feedback guidance do not carry it (`rewrites_prose`).
- The press is an Agent tab turn ("Suggest the change on line 6 of path")
  through `workshop_comment`, sent on `assistance_first_answer`: fresh, one
  tool-free call with Claude, `review.suggest-change` as the system prompt
  (profile schema 32, hook tables 0 to 14), no brief, no earlier turns. Ask
  more on the turn promotes it, as for RFC-BPR-0116 and 0117.
- Entry: **Suggest the Change** on the draft field's right-click menu, shown
  only where the draft's lines can take a suggestion (`decide_suggestion_range`
  through the window's own rule). What came of it is a panel under the field
  (the Enhance panel's look): "The agent's change is in the code…" with
  **Keep change** and **Discard**; then **Undo** and **Done**; a refusal names
  the check ("The agent's change did not apply to line 6: the text it quoted
  differs from the head.") with **Show reply** and **Dismiss**.
- An accepted reply opens RFC-BPR-0090's edit on the draft's lines and writes
  the agent's lines as one edit; while it waits, the draft keeps the lines it
  had (the body does not change), and Esc drops it. Keep change composes the
  block into the draft with the words as written; Undo restores the body
  exactly as it was before Keep change.
- RFC-BPR-0125 milestone 2 (merged into the lane at `8ba69de50`): Suggest the
  change is not an Agent tab chip; it is a candidate quick ask for the Ask
  model and is not registered as a shipped macro yet, because a macro is a
  template sent as a question and this action needs the draft's lines and its
  own structured reply. Recorded for the owner.
- Record: [verification](../verification/RFC-BPR-0118.md).

## Acceptance criteria

- **AC01:** Suggest the change is offered only on a draft comment whose lines can take a suggestion under RFC-BPR-0090's range rules, and a press sends the comment workshop entry point with the new operation and exactly those lines.
- **AC02:** A reply whose stated current text equals the head's lines opens as an in-pane suggestion edit showing the agent's replacement, and the draft comment is unchanged until the reviewer keeps it.
- **AC03:** A reply whose range differs, whose stated current text differs by any byte, whose replacement is identical or out of bounds, or which GitHub could not take as a suggestion is refused with that reason and never shown as a suggestion; a head that moved the lines during the run is such a refusal.
- **AC04:** Keeping the suggestion adds the suggestion block to the draft and leaves its text as written; one Undo restores the draft; nothing is sent to GitHub.
- **AC05:** The operation is briefed by its own built-in prompt, whose edit reaches the next Suggest the change and no other prompt.
- **AC06:** On a frozen build with a fixture agent, the owner accepts the control, an applied suggestion and a refused one.

## Proposed delivery

[Suggest the change](../issues/40-parked/BPR-2078-suggest-the-change-from-a-draft-comment.md)
and [closeout](../issues/10-backlog/BPR-2079-close-rfc-0118.md) form the third
[milestone](plans/agent-assistance-v1/milestone-plan.json).
