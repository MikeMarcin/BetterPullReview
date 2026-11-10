---
id: "RFC-DRAFT-0004"
title: "One set of conventions for every agent chat: references, send key, cost and colour"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, notes 5, 11, 12, 14 and 27","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
---

# One set of conventions for every agent chat: references, send key, cost and colour

## Problem and grounded current behavior

Four things behave differently from one agent surface to the next
([notes 5, 11, 12, 14 and 27](SOURCE_NOTES.md)).

**References.** The Agent tab already turns `path:34`, `path:34-36`, `path
line 34` and `path lines 34–36` into links that open the line in Review
(`agent_line_references`,
[agent_tab_view.hpp](../../../../src/adapters/qt/include/bpr/qt/agent_tab_view.hpp);
shipped since 0.4.0). The owner still meets many references that are not
links. The code leaves these unlinked: a path that is not one of the pull
request's changed files (`review_file_named` in
[workbench_window_comments_and_threads.cpp](../../../../src/adapters/qt/src/workbench_window_comments_and_threads.cpp)
searches only that list), a file name that two changed files share, any
reference inside a code span that holds more than the reference, any other
written form, and every reply in the tour side chat, which has no reference
links at all. Which of these the owner met was not reproduced.

**Send key.** The tour side chat sends on Return and starts a line on
Shift+Return. The Agent tab's composer sends only on Ctrl or Cmd with Return
([agent_tab_view.cpp](../../../../src/adapters/qt/src/agent_tab_view.cpp),
`eventFilter`). The selected-lines side chat composer
([side_chat_panel.cpp](../../../../src/adapters/qt/src/side_chat_panel.cpp))
binds no key.

**Cost.** Claude's result frame carries `total_cost_usd`; it is kept as the
harness's own decimal string
([agent_launch.cpp](../../../../src/application/src/agent_launch.cpp)) and
appended unrounded to the Agent tab's turn footer, as in "answered in 1 min
12 s for $0.4123567." The code treats it as the turn's cost; the owner
observes it growing across a session. Which it is on a resumed native
session was not established. The tour side chat receives results from the
same loop and shows no cost. No cost is parsed for Codex.

**Colour.** The agent accent
([agent_feature.cpp](../../../../src/adapters/qt/src/agent_feature.cpp),
light `#4A3FD6`, dark `#A3A0FF`) is applied in the tour editor, the tour
side chat and the tour agent panel. The Agent tab, its Send and Ask the
agent, and the Review Cycles send carry none. Accept, Accept all, Undo reset
and Move in the tour carry it although they start no run.
[shared-action-buttons.md](../../../ux/shared-action-buttons.md) does not
mention the accent. The two values are fixed in code and chosen only by
whether the window is light or dark. The accent is not one of the palette
roles that Appearance › Colours lets a reviewer change per theme under
[RFC-BPR-0034](../../RFC-BPR-0034-appearance-page-and-per-theme-colours.md),
although the owner expected it to be.

## Proposed behavior and boundaries

These rules apply to the Agent tab, the selected-lines side chat and the
tour side chat. The frames are in the design round:
[Agent tab](../../../design/v0-5-0-targets-v1/index.html#agent) and
[colour](../../../design/v0-5-0-targets-v1/index.html#indigo).

### Code references

One recogniser, decided in the core, serves every agent reply and question,
and the tour texts of
[RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md).

- **Forms.** `path:12`, `path:12-34`, `path:12:5` (a column, ignored),
  `path#L12`, `path#L12-L34`, `path line 12`, `path lines 12–34`, `path
  (lines 12–34)`, each also "before the change" for the base side, with a
  hyphen, an en dash or "to" between the numbers. A bare "line 12" names the
  one file the question or the stop is about, as today.
- **Where.** In prose, in emphasis, and in an inline code span when the
  span's whole content is the reference. Fenced code is left as written.
- **Resolution.** A path resolves to a changed file first, by exact path
  and then by unique suffix; then to any file of the repository at the pull
  request's head. A changed file opens in Review at the line, with the range
  revealed. An unchanged file opens wherever the application can show an
  unchanged file at that line; if it cannot, the reference is not a link.
- **Ambiguity.** A name that matches several files is a link that offers
  those files to choose from. A path that matches nothing is not a link.
- **Honesty.** A link is drawn only for a reference that will open. Copy
  and Save as note keep the text as the agent wrote it.

### Send key

In the Agent tab's composer and the selected-lines side chat's composer,
Return sends and Shift+Return starts a new line, as the tour side chat does.
Ctrl or Cmd with Return also sends. Return does not send while an input
method is composing or while the composer is empty. The tooltips say so.

Composers that save or publish text (comments, replies, notes, stop text,
descriptions) keep Ctrl or Cmd with Return. They are not chats.

### Cost

- **Format.** Dollars and cents, rounded half up: `$0.41`. A nonzero amount
  under half a cent reads `under $0.01`. No cost is shown when the runtime
  reported none; `$0.00` is never invented.
- **Per turn.** A turn's footer gives that turn's cost after its duration:
  "Claude answered in 1 min 12 s · $0.41".
- **Per session.** The session line under the composer gives the session's
  total: "$2.37 this session". It persists with the session's history and
  restarts at New session.
- **Which is which.** The core holds both numbers. How the turn's cost is
  derived from what the runtime reports (as given, or as the difference of
  consecutive session totals) is established against the installed Claude
  on a resumed session and recorded; the sum of a session's turn costs
  equals its total.
- **Where.** The Agent tab and the tour side chat show both. The
  selected-lines side chat shows the turn's.

### Colour

A control wears the agent accent exactly when pressing it starts or
continues an agent run. Everything else uses the ordinary button roles, even
when what it handles came from an agent.

- **Gains the accent:** the Agent tab's Send, Ask the agent, the suggestion
  rows, Resend, Ask follow-up where it sends, the comment workshop's
  agent-invoking actions, and the Review Cycles Send to Agent.
- **Loses the accent:** Accept, Accept all, Undo reset, Move, and any other
  control that only applies, moves, keeps or discards.
- **Not a control:** a proposal's outline and its title keep the accent.
  They say where the text came from.
- **Stop** uses the ordinary role: it ends spending.

The rule and its test are added to
[shared-action-buttons.md](../../../ux/shared-action-buttons.md) as the
agent variant of the shared buttons, drawn in the Agent accent role defined
below and its disabled form.

### The accent is an Appearance colour

The owner settled this on 2026-11-10: the accent is configurable and indigo
is only its default.

- **Agent accent** joins the palette's roles, as State attention did under
  RFC-BPR-0034. Each theme variant has its own built-in value: `#4A3FD6` for
  Light and `#A3A0FF` for Dark, the values shipped today, and for the two
  high-contrast variants an indigo that meets that variant's contrast rule.
- Appearance › Colours offers it with the other roles, with the same
  preview, Keep changes, Reset, Import and Export. A stored palette or theme
  document without the role reads it as that variant's built-in value, and
  the existing contrast check on import covers it.
- Every place that wears the accent reads the role: controls that start a
  run, a proposal's outline and title, the running block's marker, and the
  tour's agent marks. No surface keeps a fixed hex.
- The disabled form is derived from the role's current value, as it is
  derived from the fixed value today.

"Indigo" in this pack names the default, not the rule. The rule is the
accent.

### Boundaries

- The recogniser adds no network or file-system effect; resolution uses
  what the session already holds.
- Cost is the runtime's own report. BPR does not price tokens.
- The accent rule changes colour only: no control is added, removed or
  renamed by it. The one addition is the Agent accent row on Appearance ›
  Colours.

## Acceptance

- **AC01:** Each listed form, in prose, in emphasis and as a whole code
  span, becomes a link in an Agent tab reply, a selected-lines side chat
  reply and a tour side chat reply, and opens the named line with its range
  revealed; the same text inside fenced code does not.
- **AC02:** A reference to an unchanged file of the head opens that file at
  the line or is not drawn as a link; a name shared by two files offers
  both; a path that matches nothing is plain text.
- **AC03:** Return sends and Shift+Return adds a line in the Agent tab and
  the selected-lines side chat; Return during input-method composition does
  not send; a comment composer still requires Ctrl or Cmd with Return.
- **AC04:** Reported costs of 0.4123567, 0.004 and 12 are shown as `$0.41`,
  `under $0.01` and `$12.00`; a run with no reported cost shows none.
- **AC05:** After three turns in one native session with an installed
  Claude, each turn's footer shows a cost, the session line shows their sum,
  and the record states how the runtime's figure was interpreted.
- **AC06:** The tour side chat shows the turn and session costs under the
  same rules.
- **AC07:** An inventory of every control that dispatches an agent run and
  every control that wears the accent shows the two sets are equal, in the
  Agent tab, the side chats, the tour, the comment workshop and Review
  Cycles.
- **AC08:** The owner accepts the Agent tab's accent and footers on a frozen
  build.
- **AC09:** Appearance › Colours lists Agent accent; with nothing customised
  every accented surface shows the shipped Light and Dark values; changing
  it for one theme changes every accented surface in that theme and no
  other theme; Reset restores the built-in value; and a theme document
  written before the role existed imports with the built-in value.
