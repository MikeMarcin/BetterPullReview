---
id: "RFC-DRAFT-0023"
title: "One drawer design system: Notes, Tours, Agent, Compose review and the Overview panel"
status: "Proposed"
metadata: {"design_card":"BPR-1574","design_round":"docs/design/BPR-1574-drawers-and-agent-v1/index.html","base":"928dd4191","origin":"Owner, 2026-10-17 (chat, relayed by the coordinator)"}
---

# One drawer design system: Notes, Tours, Agent, Compose review and the Overview panel

Draft for [[BPR-1574]]'s design phase. Nothing here is adopted. The five
surface choices are open questions for the owner (below); once answered, this
draft becomes the decided design and `implement-rfc` adopts it.

## Problem and grounded current behavior

The owner, 2026-10-17:

> I think that comments drawer is beautiful now, but the notes and tours
> drawers are pretty ugly, and I think agent probably deserves a first-class
> tab, not to be tucked away.

The same day the owner added Compose review ("I'm not sure what's going on on
the left side of compose review. Is that supposed to be useful? I don't think
I've ever done more than try to ignore it.") and the Overview's side panel
("The right pane overview is getting extremely busy, and this is a tame
example. … It looks like a drawer but doesn't behave like the other drawers").

Before evidence: offscreen renders of `928dd4191` (tests-on Release build,
`inline-drawer-v1` and `conventional-comments-v1` fixtures, 1440 × 900, Light
and Dark) in
[`docs/design/BPR-1574-drawers-and-agent-v1/before/`](../../../design/BPR-1574-drawers-and-agent-v1/before/).

### What makes the Comments drawer work

Accepted as [[BPR-1290]] v005 (session `f76a3bf8…`, 2026-10-06) and
specified in the workbench specification's
[Conversation drawer](../../../ux/workbench-specification.md#conversation-drawer):

- **One job.** This file's threads beside their lines; the thread map, path
  caption, post list and body pane were removed. Finding a thread anywhere is
  Threads ([[BPR-1293]]).
- **The header is the tabs.** No caption; the 34-pixel tab row carries
  Comments with its blue count pill, and the tab's tools (Only new, Hide
  resolved, Next new) are icons at its right with tooltips.
- **One card anatomy.** A closed note is flat: 18-pixel avatar, the
  [[BPR-1083]] 16-pixel mark under it, a 2-pixel rail in the label's tint
  (4 pixels when blocking), two lines of body. An open thread is the panel
  colour with a shadow, a 3-pixel amber edge and a notch pointing at its line;
  the line is washed amber. My unpublished draft is dashed.
- **Colour has meaning.** Amber is where you read, blue is new and where you
  write ([[BPR-1212]]), green is resolved; everything else is a neutral ink.
  Every value is a theme role, so Light, Dark and high contrast follow.
- **Spatial.** Notes move with the code; edge chips count threads out of view;
  scroll-bar ticks and gutter cells lead back to them.
- **Few words, real keys.** Icons with tooltips; Up, Down, Return, Esc, R, N,
  Shift+F10.

### Notes today

`workbench_window.cpp` builds the Notes tab as a purpose sentence
(`workbenchNotesPurpose`) over a nested `QTabWidget` (`privateConversationTabs`:
Private history, Agent tools). Agent tools stacks `private_notes_panel`,
`agent_launch_panel` (capped at 300 pixels), `side_chat_panel`, four stacked
`QToolButton`s (Send, Add context, Start a new session, Continue against the
old revision), `finding_queue_view` and six more stacked buttons. Private
history (`private_review_panel`, RFC-BPR-0023) adds a third tab row (History,
Sources, Write) and paging buttons. The render shows the Tour tab clipped to
"T" beside it, and labelled fields running off the drawer's edge. Notes became
"your private notes and AI conversations" under [[BPR-1204]] (owner,
2026-09-25: "Findings is a bad heading for AI -- may as well just call it AI
or Agent; if the UX even makes sense").

### The Tour tab today

`review_tour_editor` puts RFC-BPR-0039's whole authoring flow into the 375-pixel
column: a paragraph, the GitHub tour import list, a second tab row (Problem,
Solution, Tour stops, Preview) of form fields and help text, status
paragraphs and a stack of conflict and publication buttons. Nothing in it
shows the code a stop points at, although RFC-BPR-0039 asks to "keep the code
and explanation visible together, with the ordered outline beside them".

### Agent surfaces today

- Launch, side chat and findings (RFC-BPR-0012, [[BPR-0502]], [[BPR-0513]],
  [[BPR-0514]]): Notes › Agent tools, two levels down.
- Private runs on drafts (RFC-BPR-0023, [[BPR-1245]]): Notes › Private history.
- Comment workshop (RFC-BPR-0012, [[BPR-0966]]): inside the composer.
- Tour assistance (RFC-BPR-0043): Enhance and Suggest outline in the tour editor.
- Review Cycles handoff (RFC-BPR-0053 to RFC-BPR-0058): the share sheet's Agent.
- Profiles and context: Settings › Agents & context (RFC-BPR-0011, RFC-BPR-0060).

`showWorkbenchAiToolsAction` is labelled "Notes"; `keyboard_shortcuts.cpp` has
no agent command and no menu names the agent.

### Compose review today

`apply_modality_surface()` shows any composition page in Discuss and moves
Discuss's `github_conversation_panel` into a 340-pixel column beside it
(`discuss_composer_list_width`). [[BPR-1212]] did this so a maximized draft is
written beside its neighbours. Compose review (⌥R, Finish review) inherited the
column: the owner's Q3 answer ("A: move to Discuss") was asked as "it moves
into Discuss beside the drafts", and the start-a-review frames drew only the
pending comments beside the summary. What is built is every thread of the pull
request, with Discuss's scope and filters, while the Finish page itself already
lists the pending comments ([[BPR-1345]]). [[BPR-1541]]'s accepted waiting
panel sits under the page title.

### The Overview's side panel today

`pull_request_overview_page.cpp`'s summary column, a fixed widget rather than a
`workbench_drawer_panel`: Merge and Waiting on (RFC-BPR-0069, [[BPR-1405]]),
Requirement details, Participants, Since my last review and My review
(RFC-BPR-0028, [[BPR-0647]]), Start/Continue review, Check details with its
result list and detail box and Open check details (RFC-BPR-0068), Prepare
review tour (RFC-BPR-0039). A failed check is named in Merge, counted in Since
my last review and listed in Check details. Rows are drawn for "—", "0" and
"not available". Disclosures are full-width grey bars ([[BPR-1579]] is fixing
their Dark contrast; [[BPR-1577]] the Open/All switch).

**Start review.** `overviewEntryButton` calls `enter_overview_at`, which since
[[BPR-1295]] only calls `enter_review_at(EntryPath)`: it enters Review and
selects the first unseen change's file (`pull_request_overview_projection::
EntryPath`). It is the page's default button, so Return on the Overview
presses it. It starts no session and records nothing; "my last review" comes
from receipts and epochs, not from the button.

## Proposed behavior and boundaries

### Decided

- **D1, owner 2026-10-17:** the Overview has no Start review (or Continue
  review) button in any direction: "Start review is probably unnecessary now
  that the Review tab is so convenient, and double-clicking a file or clicking
  Next file also works." Two things the button did are kept elsewhere: the
  first entry into Review (⌘2 or the Review segment) from the Overview of a
  pull request in which no file has been chosen in this window selects
  `EntryPath`, the first unseen change's file; and Return on the Overview's
  Files list opens the selected file, as a double-click does. The courses that
  press `overviewEntryButton` move to those gestures.

### Principles (proposed; owner question Q0)

Every drawer and side surface follows these, drawn on the
[principles board](../../../design/BPR-1574-drawers-and-agent-v1/principles.html):

1. **The header is the tabs.** No caption, no purpose paragraph, no tabs
   inside a tab. Tools are icons at the tab row's right with tooltips naming
   their keys. Purpose lives in the tooltip and the empty state.
2. **One card anatomy** (the Comments drawer's), for threads, notes, tour
   stops, findings and pending comments.
3. **Ownership at a glance.** Solid is published; dashed is yours and
   unpublished; a lock is private forever; a square avatar with a prompt
   glyph is an agent, dashed until a person promotes it; a numbered amber
   circle is a tour stop.
4. **Colour means one thing each**: amber where you read, blue new and where
   you write, green done or passing, red blocking, failed or destructive
   only, grey otherwise. Theme roles only; no new hues.
5. **Actions:** at most one filled and one outlined button per card or page;
   the rest in More and the context menu (Shift+F10). No stacked button
   columns, no full-width grey bars.
6. **States:** empty is a glyph, one sentence and one action; working is a
   quiet line with Stop; failed says what happened, what is kept and how to
   try again. No row is drawn to say 0, — or not available.
7. **Every drawer behaves the same:** resizable by its edge (280 to 760
   pixels; a Status drawer on the Overview from 260), collapsible from its
   toggle and key, width remembered per window as the files and comments
   drawers are ([[BPR-0662]]), its tabs one focus stop with Left/Right, its
   cards on Up, Down, Return and Esc, one letter for its main verb, each tab
   scrolling on its own.
8. D1 above.

### Open choices (owner questions; affected scope unready until answered)

Each is a choice between named directions, drawn in Light and Dark at
[the design round](../../../design/BPR-1574-drawers-and-agent-v1/index.html).
A recommendation is not an answer.

- **Q1, Notes** ([notes.html](../../../design/BPR-1574-drawers-and-agent-v1/notes.html)).
  In every direction Notes holds only your private notes; agent material
  moves to the Agent surface.
  - **A. Margin notes:** notes hang from their lines like threads, private by
    dashed edge and lock; pull-request and other-file notes wait in edge
    chips. Requires a line anchor for private notes in the ledger (core and
    host work; today a note attaches to a file comparison or the pull request).
  - **B. Notebook:** one list, this file first, then the pull request, then
    other files; one field starts a note; Make it a comment on each. Fits
    today's data.
  - **C. Scratchpad:** one private Markdown page per pull request with
    `file:line` reference chips and selection-to-comment. Replaces discrete
    notes (a model change; per-note delete and promote go).
- **Q2, Tours** ([tours.html](../../../design/BPR-1574-drawers-and-agent-v1/tours.html)).
  - **A. The route:** brief card, then numbered stops on an amber route line;
    the current stop opens in place and washes its lines; Preview and publish
    in the foot.
  - **B. Stops at their lines:** numbered stop cards hang from their lines like
    threads, a route stepper on top; brief and publish on a Discuss page.
  - **C. Tour page and player:** authoring in the main pane (outline, stop
    editor with its code excerpt, live GitHub preview); the drawer's Tour tab
    becomes a reader's player.
- **Q3, Agent** ([agent.html](../../../design/BPR-1574-drawers-and-agent-v1/agent.html)).
  Every direction gathers launch, chat, findings and private runs in one
  place, shows what the agent sees and that it cannot act, offers ⌘I (Ctrl+I)
  "Ask about the selected lines" from the code, and has the same four states:
  not set up (Set up an agent opens Settings › Agents & context), working
  (Stop), head moved (Start a new session / Continue on the old head, the
  core's two continuations), stopped (Ask again, Show the log).
  - **A. A fifth modality, Agent (⌘5):** sessions rail, conversation with
    finding cards, context rail; drawers hidden as in Discuss and Threads.
  - **B. A drawer tab, Agent (⌥⇧A):** beside Comments, Notes and Tour; the code
    stays in view, a finding's lines wash amber, its gutter mark is a square.
  - **C. A dock under the code:** a title-row Agent button opens a resizable
    dock with conversation and findings side by side; findings mark the gutter
    and scroll bar.
- **Q4, Compose review** ([compose.html](../../../design/BPR-1574-drawers-and-agent-v1/compose.html)).
  BPR-1541's waiting panel stays under the title, unchanged, in each.
  - **A. Focused:** one column; Discuss's list is not shown while composing;
    an unsent draft or unread replies are one sentence with one fix.
  - **B. What will be published:** the left column becomes a manifest (tally
    by label, files, each pending comment as a note, what is not in the
    review); summary and verdicts on the right.
  - **C. A sheet over the code:** Finish review opens a sheet over Review; this
    reverses the owner's BPR-1212 Q3 answer and so needs that answer changed.
  - In A and B a maximized draft ([[BPR-1212]]) keeps its list beside it; only
    Compose review changes.
- **Q5, Overview panel** ([overview.html](../../../design/BPR-1574-drawers-and-agent-v1/overview.html)).
  - **A. Attention first:** a Status drawer that leads with what needs you
    (conflicts, failed checks once, waiting on, replies to you), then your
    progress, one checks bar and the people as faces.
  - **B. Tabs:** Summary, Checks (failures first, one detail card), People.
  - **C. Status band and checks drawer:** a band of chips over Files; choosing
    a chip opens its detail (Checks as a drawer, replies in Threads).

### Boundaries

Presentation in the Qt adapter, except Q1 A's note anchor and Q1 C's single
page, which need core and host changes. The core decides, the host performs,
Qt presents: no widget gains a GitHub, Git, ledger, agent, process or network
effect; agents stay read-only (RFC-BPR-0011, RFC-BPR-0012, RFC-BPR-0060) and
nothing an agent says is published without an explicit person's action.
Publication, review submission, check admission and their accepted wording
are unchanged. The Comments drawer and BPR-1541's panel are not changed. The
`frontend-design` skill is loaded for every build slice, and each surface's
owner look is an attended session on a frozen build, one question per
criterion.

### Milestones after the answers

1. Shared primitives (drawer header, card, ownership marks, empty/working/
   failed states) and the Overview column as a real drawer (D1 included).
2. Notes (Q1). 3. Tours (Q2). 4. Agent (Q3). 5. Compose review (Q4).
6. Overview panel content (Q5).

Each ends in its own owner look. Milestones 2 to 6 are independent once 1 lands,
except that Notes loses its agent material only when Agent's surface exists.

## Acceptance

- **AC01 Principles:** each rebuilt surface passes the eight principles, with
  a negative control per principle that a course can fail (a nested tab, a
  stacked button column, a row reading "not available", a colour outside the
  theme roles).
- **AC02 Notes:** the tab shows only private notes in the chosen Q1 direction;
  Make it a comment copies a note to a local draft at its line or file and
  publishes nothing.
- **AC03 Tours:** a tour is authored, reordered by keyboard and pointer,
  previewed and followed in the chosen Q2 direction, with code and explanation
  visible together.
- **AC04 Agent:** the agent is reached in one gesture (its key, its tab or
  segment, ⌘I) in the chosen Q3 direction; the four states render; a finding
  becomes a local draft only on a person's press.
- **AC05 Compose review:** in the chosen Q4 direction no unrelated thread list
  is shown, every pending comment and blocking count is visible before a
  verdict, and BPR-1541's panel is unchanged.
- **AC06 Overview:** in the chosen Q5 direction each failed check is shown in
  one place, no empty row is drawn, there is no Start review button, the side
  surface resizes, collapses and remembers its width, and ⌘2 from a fresh
  Overview lands on the first unseen change.
- **AC07 Themes and sizes:** every rebuilt surface renders without clipping in
  Light, Dark and both high-contrast themes at 100, 125 and 200 percent and at
  its narrowest and widest drawer width.
- **AC08 Owner looks:** the owner accepts each rebuilt surface on a frozen
  build.
