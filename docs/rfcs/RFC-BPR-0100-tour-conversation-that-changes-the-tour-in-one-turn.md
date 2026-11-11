---
id: "RFC-BPR-0100"
title: "A tour conversation that changes the tour in one turn"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, notes 4, 8, 9 and 10","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# A tour conversation that changes the tour in one turn

## Problem and grounded current behavior

The owner finds the bottom of the tour side chat cluttered, wants the whole
tour's chat to propose a change as directly as a stop's chat does, wants
Enhance to run in the part's own conversation, wants code references in
stops to be links or snippets, and wants stops to stop narrating behaviour
that a change only moved behind a flag
([notes 4, 8, 9 and 10](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). In the inspected code:

- Under the transcript,
  [tour_side_chat.cpp](../../src/adapters/qt/src/tour_side_chat.cpp)
  stacks a permanent "Update … from this chat" button, a History and New
  session row, a continuation label, a revision label, a "Continue old
  revision" button when the head moved, the passage row, and the composer.
  The button was added on 2026-11-06 and the session rows on 2026-11-08.
- The blue link the owner prefers is "Nothing in the tour changed. If this
  answer describes a change, Update the tour from this chat asks the agent
  for it." It is shown only in the whole-tour thread, under the newest
  answer that proposed nothing, and it clicks the same Update button.
- A stop, Problem or Solution thread answers with `bpr.tour-ask.v1`; a
  proposal there is a card in the chat with Accept, Edit text and Discard.
  The whole-tour thread answers with `bpr.tour-revise.v1`; its proposals
  wait in the tour marked grey, and the chat shows one line saying so
  ([tour_agent_conversation.cpp](../../src/application/src/tour_agent_conversation.cpp)).
  Both share the instruction "Change the tour only when the author asks for
  a change." Why the whole-tour thread so often answers in words first was
  not reproduced; the link above was added for that case.
- Enhance dispatches a writing hook: a one-shot, tool-free request under
  `review-preparation-v1` that returns a replacement for one field or
  selection. It shares nothing with the part's side chat session.
- The writing guidance
  ([tour_writing_style.cpp](../../src/application/src/tour_writing_style.cpp),
  [tour-stop-comment.md](../../src/application/resources/review-preparation/tour-stop-comment.md),
  and the guide `bpr.tour-guide.v1` in
  [tour_agent_context.cpp](../../src/application/src/tour_agent_context.cpp))
  says nothing about how to cite code and nothing about moved or flag-gated
  code. A drafted stop is free to write "(lines 40–52)".

## Proposed behavior and boundaries

The frames are in the
[design round](../design/v0-5-0-targets-v1/index.html#chat).

### What sits under the transcript

Under the transcript there is the composer and one line.

- The composer holds the passage, when one is attached, as a removable chip
  inside the field.
- The line shows a **Session** menu on the left (the session's ordinal;
  History and New session are its items) and the session's spend on the
  right, as [RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md)
  defines it.
- The permanent Update button is removed.
- Continuation is stated only when it is a transcript replay. The revision
  notice and Continue old revision appear only when the head moved. Both sit
  above the composer as a notice, not as standing rows.

### Asking for the update

Under the newest answered turn that proposed nothing, the chat shows
"Nothing in *the part* changed. Update *the part* from this chat", the last
four words a link, where the part is this stop, the Problem, the Solution or
the tour. This holds in every thread. The link sends the existing update
turn. It is the only place the update is offered.

### A change request is answered with the change

In the whole-tour thread, a message that asks for a change (fewer stops, a
merge, an order, a focus, a rewrite) is answered with that revision in the
same turn. The conversation contract states this as the rule for a request,
and keeps "answer in words" for a question.

A revision is also shown in the chat, as a card that lists its operations in
plain words (merge, remove, move, new, rewritten) with three actions:
**Accept all**, **Review each in the tour** (the grey markers that exist
today) and **Discard**. Accepting or discarding in either place settles the
card in the other. Accept all puts the changes in the draft; each new or
changed stop still needs its own acceptance under
[RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md), while Accept on one stop's own proposal accepts
that stop.

### Enhance runs in the part's conversation

Enhance on the Problem, the Solution, a stop or a selection opens that
part's side chat and sends its update turn in that part's session, with the
session's history and its read-only tools. The result is a proposal card.
For a selection the proposal replaces only the selection. Nothing is written
to the tour until the author accepts.

The one-shot writing hooks remain for the `bpr tour` CLI and the convention
hooks of RFC-BPR-0051; the application's Enhance buttons stop using them.

### Code references in stops

- The drafting and conversation guidance tell the agent to cite code as
  `path:first-last`, to cite only lines the reader should look at, and never
  to restate the stop's own range, which the stop already shows.
- In the application, a reference in a stop's text, the Problem or the
  Solution is a link under the shared rules of RFC-BPR-0101. A reference
  alone on its own line is shown as a snippet of those lines at the tour's
  pinned head commit, at most 12 lines, with Show in Review.
- On GitHub, an inline reference is published as a Markdown link to a
  commit-pinned permalink, and a reference alone on its own line as that
  bare permalink, which GitHub draws as an embedded snippet where it
  supports that. Whether review comments and conversation comments both
  embed is to be established on MikeMarcin/BPR-Test before the published
  form is fixed; where GitHub does not embed, the link is the form.
- A reference the tour's head cannot resolve is left as written and is
  reported to the author before publication as text that will not link.

### Moved and flag-gated code

The guidance gains this rule, in the drafting prompt, the conversation
contract and the stop-comment hook: when a change moves existing behaviour
unchanged into a branch (behind a flag, an option, a platform check), say
that the existing path is kept and where the condition is; do not describe
what the existing path does. Explain the new path and what selects it.

The guide and the hook guidance are versioned; both versions advance with
these rules.

### Boundaries

- Sessions, history, New session, replay and the read-only tool list are
  RFC-BPR-0096's, unchanged.
- Acceptance stays the author's act. No turn writes to the tour or to
  GitHub by itself.
- An agent may still answer a change request in words. The link remains for
  that case; this RFC lowers how often it is needed and does not promise it
  away.

## Acceptance criteria

- **AC01:** With a native session and an unmoved head, nothing but the
  composer and the session line is shown under the transcript; a replay or a
  moved head each adds exactly its one notice.
- **AC02:** In each of the four thread kinds, an answer that proposed
  nothing is followed by the link naming that part, and following it sends
  the update turn. No other control offers the update.
- **AC03:** The whole-tour request and response contracts state that a
  change request is answered with its revision in that turn; with an
  installed agent on a fixture tour, each of five fixed change requests
  yields a proposal card in its first answer.
- **AC04:** Accept all applies every operation of a revision; Review each
  leaves them grey in the tour; settling the revision in the tour settles
  the chat card.
- **AC05:** Enhance on a stop with prior conversation sends a turn in that
  stop's session, and its proposal changes the stop only on Accept. Enhance
  on a selection replaces only the selection.
- **AC06:** A stop text containing `path:first-last` shows a link that opens
  those lines in Review; the same reference alone on a line shows the
  snippet. The published comment contains the commit-pinned permalink in the
  form established on BPR-Test.
- **AC07:** The citation rule and the moved-code rule are present in the
  drafting prompt, the conversation contract and the stop-comment hook, and
  the guide and hook versions differ from the inspected ones.
- **AC08:** On a fixture pull request that moves an unchanged path behind a
  flag, the owner accepts that a drafted tour's stops do not describe the
  retained path's behaviour.
- **AC09:** The owner accepts the side chat's lower edge and the revision
  card on a frozen build.
