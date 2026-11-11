---
id: "RFC-BPR-0102"
title: "An agent question says what it points at, and a suggestion always sends"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, note 17","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# An agent question says what it points at, and a suggestion always sends

## Problem and grounded current behavior

From Overview, the suggestion "Review this pull request" answers "Not sent.
Select the lines to ask about in the diff, then send." The owner does not
understand the field whose tooltip reads "What the agent may read", cannot
choose "nothing extra", cannot clear a selection, and got a one-file answer
to a whole pull request review with "Pull request" chosen
([note 17](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). The inspected code explains each:

- Every send from the Agent tab uses the entry point `ask_about_selection`
  ([window_agent_chat.cpp](../../src/app/src/github_application/window_agent_chat.cpp),
  [agent_chat_session_host.cpp](../../src/application/src/agent_chat_session_host.cpp)).
  `decide_agent_action` in
  [agent_chat_scope.cpp](../../src/core/src/agent_chat_scope.cpp)
  returns `selection_required` for that entry point without a valid
  selection. Suggestions only supply text
  ([agent_tab_view.cpp](../../src/adapters/qt/src/agent_tab_view.cpp),
  `refresh_suggestions`), so with nothing selected every one of them is
  refused. An entry point that needs no selection, `run_ai_review`, exists
  and is not used by them.
- The scope list offers Selected lines, This file, Pull request and
  Repository (`agent_chat_scope_kind`,
  [agent_chat_scope.hpp](../../src/core/include/bpr/core/agent_chat_scope.hpp)).
  The dispatch that builds the request forwards the message, notes, draft,
  session and entry point, and no scope. In the inspected paths the choice
  changes neither the prompt, nor the files the context helper serves, nor
  the workspace. It is recorded and displayed.
- What is actually sent is fixed: a brief of the pull request (title,
  description, base, the full file list, threads), the open file as the
  request's only `file` entry, and the selection's text when there is one.
  The agent then reads whatever it wants from its checkout. An agent told
  `file.count` is 1 can reasonably conclude one file is in scope; that is
  the likely origin of the one-file review, though the reply itself was not
  reproduced.
- Nothing in the diff view clears a line selection. Escape is not handled
  there.

RFC-BPR-0012 says "Navigation does not silently add context. Expanding
scope or adding private notes/drafts requires explicit action."

## Proposed behavior and boundaries

The frames are in the
[design round](../design/v0-5-0-targets-v1/index.html#agent).

### A question has a subject

Each request states its **subject**, decided by the core from what the
reviewer did:

| Subject | When | What the agent is told |
|---|---|---|
| The pull request | Nothing is attached, or a suggestion names the pull request | The question concerns the whole change; every changed file is in scope |
| A file | A suggestion names the open file | That file is the subject; the rest of the change is context |
| Lines | Lines are attached | Those lines are the subject, with their text |
| A conversation | Asked from a thread (RFC-BPR-0094) | Unchanged |

The open file and the reading position are sent as **where the reviewer is
looking**, named as such and separate from the subject. They never narrow a
question whose subject is the pull request.

### The attachment replaces the scope list

The scope list and its tooltip are removed. In their place:

- When lines are selected in the diff, the composer shows them as a chip
  (`renderer.cpp 5–8`). The chip is the selection: the question's subject
  is those lines.
- The chip's ✕ detaches the lines from this question without changing the
  diff's selection. A new selection attaches again.
- With no chip, the question's subject is the pull request.
- The composer's tooltip says what is true: the agent can read the whole
  pull request; attached lines only tell it where you are looking. (Owner
  amendment, 2026-11-10, [[BPR-2038]]: it was one line under the composer,
  and "this agent can read whole PR thing is way too long ... needs to be a
  tooltip message instead".)

Private notes and drafts are still added only by their own explicit
actions.

### Suggestions carry their subject

A suggestion is a subject and a text. "Review this pull request" is sent
with the pull request as subject and the review entry point, with or
without a selection and from any mode including Overview. "Review *file*"
names that file. "Explain lines 5–8" is offered only while lines are
selected. No suggestion is offered that cannot be sent.

A typed question with no selection is sent, with the pull request as its
subject. "Select the lines to ask about" remains only for an action that
cannot mean anything without lines, and that action is disabled, with that
sentence as its tooltip, rather than refused after the click.

### Clearing the selection

Escape in the diff clears the line selection when no popup, find bar or
editor in the diff has a use for Escape first. The chip disappears with it.

### Boundaries

- This retires the File, Pull request and Repository levels of
  RFC-BPR-0012's chat scope, which bounded nothing. What an agent can read
  is set by its workspace and its own permissions
  ([RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)),
  not by this control.
- Nothing more is sent than today: the brief, where the reviewer is
  looking, and attached lines.
- Sessions and their history are unchanged.

## Acceptance criteria

- **AC01:** From Overview with nothing selected, "Review this pull request"
  sends, and its request has the pull request as subject, the review entry
  point, no selection, and every changed file listed as in scope.
- **AC02:** With lines selected, the same suggestion sends the same request:
  the selection is neither attached nor required.
- **AC03:** With an installed agent on a fixture pull request of at least
  five changed files and one of them open, the answer to "Review this pull
  request" discusses more than the open file.
- **AC04:** Selecting lines shows the chip and a typed question's request
  carries those lines as its subject; ✕ sends the next question with the
  pull request as subject and leaves the diff's selection in place.
- **AC05:** Escape in the diff clears the selection and the chip; with the
  find bar open, Escape closes the find bar and the selection remains.
- **AC06:** No scope list is shown, and no suggestion or enabled control in
  the Agent tab ends in "Not sent. Select the lines…".
- **AC07:** A typed question with nothing selected is sent and answered.
